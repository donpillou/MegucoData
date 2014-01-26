
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#endif

#include <curl/curl.h>

#include <nstd/Debug.h>

#include "Websocket.h"

#ifdef _WIN32
#define ERRNO WSAGetLastError()
#else
typedef int SOCKET;
#define ERRNO errno
#define SOCKET_ERROR (-1)
#endif

static class Curl
{
public:
  Curl()
  {
    curl_global_init(CURL_GLOBAL_ALL);
  }
  ~Curl()
  {
    curl_global_cleanup();
  }
} curl;

Websocket::Websocket(const String& origin, bool useMask) : curl(0), s(0), origin(origin), useMask(useMask) {}

Websocket::~Websocket()
{
  if(curl)
    curl_easy_cleanup(curl);
}

bool_t Websocket::connect(const String& url)
{
  // parse url
  String path("/"), host;
  uint16_t port = 80;
  bool_t useHttps = false;
  {
    String urlNoProto = url;
    if(url.startsWith("wss://"))
    {
      urlNoProto = url.substr(6);
      port = 8080;
      useHttps = true;
    }
    else if(url.startsWith("ws://"))
      urlNoProto = url.substr(5);
    const char_t* pathStart = urlNoProto.find('/');
    if(pathStart)
    {
      host = urlNoProto.substr(0, pathStart - (const char_t*)urlNoProto);
      path = urlNoProto.substr(host.length());
    }
    const char_t* portStart = host.find(':');
    if(portStart)
    {
      port = host.substr(portStart - (const char_t*)host + 1).toUInt();
      host.resize(portStart - (const char_t*)host);
    }
  }
  String httpUrl("http://");
  if(useHttps)
    httpUrl = String("https://");
  httpUrl += host;
  if(port != (uint16_t)(useHttps ? 8080 : 80))
  {
    String portStr;
    portStr.printf(":%hu", port);
    httpUrl += portStr;
  }
  httpUrl += path;

  // initialize curl
  if(!curl)
  {
    curl = curl_easy_init();
    if(!curl)
    {
      error = "Could not initialize curl.";
      return false;
    }
  }
  else
    curl_easy_reset(curl);

  // create connection
  curl_easy_setopt(curl, CURLOPT_URL, (const char_t*)httpUrl);
  curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
  CURLcode status = curl_easy_perform(curl);
  if(status != CURLE_OK)
  {
    error.printf("%s.", curl_easy_strerror(status));
    return false;
  }
  SOCKET s;
  status = curl_easy_getinfo(curl, CURLINFO_LASTSOCKET, &s);
  if(status != CURLE_OK)
  {
    error.printf("%s.", curl_easy_strerror(status));
    return false;
  }
  this->s = (void_t*)s;

  // send header
  String buffer(1500);
  buffer += "GET "; buffer += path; buffer += " HTTP/1.1\r\n";
  buffer += "Host: "; buffer += host; if(port != 80) {String portStr; portStr.printf(":%hu", port); buffer += portStr;} buffer += "\r\n";
  buffer += "Upgrade: websocket\r\n";
  buffer += "Connection: Upgrade\r\n";
  if(!origin.isEmpty()) {buffer += "Origin: "; buffer += origin; buffer += "\r\n";}
  buffer += "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n";
  buffer += "Sec-WebSocket-Version: 13\r\n\r\n";
  if(!sendAll((const byte_t*)(const char_t*)buffer, buffer.length()))
  {
    this->s = 0;
    return false;
  }

  // receive response
  size_t totalReceived = 0;
  for(;;)
  {
    size_t received;
    if(!receive((byte_t*)(char_t*)buffer + totalReceived, buffer.capacity() - totalReceived, 1000 * 300, received))
    {
      this->s = 0;
      return false;
    }
    totalReceived += received;
    buffer.resize(totalReceived);
    const char_t* headerEnd = buffer.find("\r\n\r\n");
    if(headerEnd != 0)
    {
      recvBuffer.reserve(1500);
      recvBuffer.append((byte_t*)headerEnd + 4, totalReceived - (headerEnd + 4 - (const char_t*)buffer));
      break;
    }
    if(totalReceived == buffer.capacity())
    {
      error = "Received http response header is too large.";
      this->s = 0;
      return false;
    }
  }
  if(!buffer.startsWith("HTTP/1.1 "))
  {
    error = "Could not parse http response header.";
    this->s = 0;
    return false;
  }
  uint_t returnCode = buffer.substr(9, 10).toUInt();
  if(returnCode != 101)
  {
    error.printf("Server responded with code %u.", returnCode);
    this->s = 0;
    return false;
  }
  return true;
}

void_t Websocket::close()
{
  if(curl)
  {
    curl_easy_cleanup(curl);
    curl = 0;
  }
  s = 0;
  recvBuffer.clear();
}

bool_t Websocket::isOpen() const
{
  return s != 0;
}

bool_t Websocket::recv(Buffer& buffer, timestamp_t timeout)
{
  if(!curl)
    return false;

  struct wsheader_type {
      unsigned header_size;
      bool fin;
      bool mask;
      enum opcode_type {
          CONTINUATION = 0x0,
          TEXT_FRAME = 0x1,
          BINARY_FRAME = 0x2,
          CLOSE = 8,
          PING = 9,
          PONG = 0xa,
      } opcode;
      int N0;
      uint64_t N;
      uint8_t masking_key[4];
  };

  for(;;)
  {
    for(;;)
    {
      wsheader_type ws;
      if (recvBuffer.size() < 2) { break; /* Need at least 2 */ }
      uint8_t* data = (uint8_t*)recvBuffer; // peek, but don't consume
      ws.fin = (data[0] & 0x80) == 0x80;
      ws.opcode = (wsheader_type::opcode_type) (data[0] & 0x0f);
      ws.mask = (data[1] & 0x80) == 0x80;
      ws.N0 = (data[1] & 0x7f);
      ws.header_size = 2 + (ws.N0 == 126? 2 : 0) + (ws.N0 == 127? 6 : 0) + (ws.mask? 4 : 0);
      if (recvBuffer.size() < ws.header_size) { break; /* Need: ws.header_size - rxbuf.size() */ }
      int i;
      if (ws.N0 < 126) {
          ws.N = ws.N0;
          i = 2;
      }
      else if (ws.N0 == 126) {
          ws.N = 0;
          ws.N |= ((uint64_t) data[2]) << 8;
          ws.N |= ((uint64_t) data[3]) << 0;
          i = 4;
      }
      else if (ws.N0 == 127) {
          ws.N = 0;
          ws.N |= ((uint64_t) data[2]) << 56;
          ws.N |= ((uint64_t) data[3]) << 48;
          ws.N |= ((uint64_t) data[4]) << 40;
          ws.N |= ((uint64_t) data[5]) << 32;
          ws.N |= ((uint64_t) data[6]) << 24;
          ws.N |= ((uint64_t) data[7]) << 16;
          ws.N |= ((uint64_t) data[8]) << 8;
          ws.N |= ((uint64_t) data[9]) << 0;
          i = 10;
      }
      if (ws.mask) {
          ws.masking_key[0] = ((uint8_t) data[i+0]) << 0;
          ws.masking_key[1] = ((uint8_t) data[i+1]) << 0;
          ws.masking_key[2] = ((uint8_t) data[i+2]) << 0;
          ws.masking_key[3] = ((uint8_t) data[i+3]) << 0;
      }
      else {
          ws.masking_key[0] = 0;
          ws.masking_key[1] = 0;
          ws.masking_key[2] = 0;
          ws.masking_key[3] = 0;
      }
      if (recvBuffer.size() < ws.header_size+ws.N) { break; /* Need: ws.header_size+ws.N - rxbuf.size() */ }

      // We got a whole message, now do something with it:
      if (false) { }
      else if (ws.opcode == wsheader_type::TEXT_FRAME && ws.fin) {
          if (ws.mask) { for (size_t i = 0; i != ws.N; ++i) { data[i+ws.header_size] ^= ws.masking_key[i&0x3]; } }
          if(recvBuffer.size() == ws.header_size + (size_t)ws.N)
          {
            recvBuffer.removeFront(ws.header_size);
            buffer.free();
            buffer.reserve(recvBuffer.capacity());
            buffer.swap(recvBuffer);
          }
          else
          {
            buffer.clear();
            buffer.append((const byte_t*)recvBuffer + ws.header_size, (size_t)ws.N);
            recvBuffer.removeFront(ws.header_size + (size_t)ws.N);
          }
          return true;
      }
      else if (ws.opcode == wsheader_type::PING) {
          if (ws.mask) { for (size_t i = 0; i != ws.N; ++i) { data[i+ws.header_size] ^= ws.masking_key[i&0x3]; } }
          sendFrame(wsheader_type::PONG, data + ws.header_size, (size_t)ws.N);
          recvBuffer.removeFront(ws.header_size + (size_t)ws.N);
      }
      else if (ws.opcode == wsheader_type::PONG) { }
      else if (ws.opcode == wsheader_type::CLOSE) {
        close();
        buffer.clear();
        error = "Connection was closed.";
        return false;
      }
      else {
        error.printf("Got unknown message 0x%02x.", (int_t)ws.opcode);
        close();
        return false;
      }

      recvBuffer.removeFront(ws.header_size + (size_t)ws.N);
    }

    //
    if(recvBuffer.size() == recvBuffer.capacity())
    {
      error = "Received data frame is too large.";
      close();
      return false;
    }
    size_t received;
    if(!receive((byte_t*)recvBuffer + recvBuffer.size(), recvBuffer.capacity() - recvBuffer.size(), timeout, received))
    {
      close();
      return false;
    }
    if(received == 0)
    {
      buffer.clear();
      return true;
    }
    recvBuffer.resize(recvBuffer.size() + received);
  }
}

bool_t Websocket::send(const byte_t* buffer, size_t size)
{
  return sendFrame(0x1, buffer, size);
}

bool_t Websocket::sendPing()
{
  return sendFrame(9, (const byte_t*)"", 0);
}

bool_t Websocket::sendFrame(uint_t type, const byte_t* data, size_t size)
{
  if(!curl)
    return false;

  // TODO:
  // Masking key should (must) be derived from a high quality random
  // number generator, to mitigate attacks on non-WebSocket friendly
  // middleware:
  const uint8_t masking_key[4] = { 0x12, 0x34, 0x56, 0x78 };
  byte_t header[2 + 2 + 6 + 4];
  size_t headerSize = 2 + (size >= 126 ? 2 : 0) + (size >= 65536 ? 6 : 0) + (useMask ? 4 : 0);
  header[0] = 0x80 | type;
  if (false) { }
  else if (size < 126) {
      header[1] = (size & 0xff) | (useMask ? 0x80 : 0);
      if (useMask) {
          header[2] = masking_key[0];
          header[3] = masking_key[1];
          header[4] = masking_key[2];
          header[5] = masking_key[3];
      }
  }
  else if (size < 65536) {
      header[1] = 126 | (useMask ? 0x80 : 0);
      header[2] = (size >> 8) & 0xff;
      header[3] = (size >> 0) & 0xff;
      if (useMask) {
          header[4] = masking_key[0];
          header[5] = masking_key[1];
          header[6] = masking_key[2];
          header[7] = masking_key[3];
      }
  }
  else {
      header[1] = 127 | (useMask ? 0x80 : 0);
      header[2] = ((uint64_t)size >> 56) & 0xff;
      header[3] = ((uint64_t)size >> 48) & 0xff;
      header[4] = ((uint64_t)size >> 40) & 0xff;
      header[5] = ((uint64_t)size >> 32) & 0xff;
      header[6] = (size >> 24) & 0xff;
      header[7] = (size >> 16) & 0xff;
      header[8] = (size >>  8) & 0xff;
      header[9] = (size >>  0) & 0xff;
      if (useMask) {
          header[10] = masking_key[0];
          header[11] = masking_key[1];
          header[12] = masking_key[2];
          header[13] = masking_key[3];
      }
  }

  if(!sendAll(header, headerSize))
    return false;
  if(size > 0)
  {
    if (useMask)
    {
      Buffer buffer(data, size);
      byte_t* p = buffer;
      for(size_t i = 0; i < size; ++i)
        p[i] ^= masking_key[i & 0x3];
      if(!sendAll(data, size))
        return false;
    }
    else
    {
      if(!sendAll(data, size))
        return false;
    }
  }
  return true;
}

bool_t Websocket::sendAll(const byte_t* data, size_t size)
{
  fd_set writefds;
  FD_ZERO(&writefds);
  timestamp_t timeout = 10000;
  timeval tv = { (long)(timeout / 1000L), (long)((timeout % 1000LL)) * 1000L };
  for(;;)
  {
    FD_SET((SOCKET)s, &writefds);
    int ret = select((SOCKET)s + 1, 0, &writefds, 0, &tv);
    if(ret < 0)
    {
      error = getSocketErrorString();
      return false;
    }
    if(ret == 0)
      continue;

    size_t sent, totalSent = 0;
    CURLcode status;
    do
    {
      status = curl_easy_send(curl, data, size, &sent);
      if(status != CURLE_OK)
      {
        error.printf("%s.", curl_easy_strerror(status));
        return false;
      }
      totalSent += sent;
    } while(totalSent < size);
    return true;
  }
}

bool_t Websocket::receive(byte_t* data, size_t size, timestamp_t timeout, size_t& received)
{
  fd_set readfds;
  FD_ZERO(&readfds);
  timeval tv = { (long)(timeout / 1000L), (long)((timeout % 1000LL)) * 1000L };
  for(;;)
  {
    FD_SET((SOCKET)s, &readfds);
    int ret = select((SOCKET)s + 1, &readfds, 0, 0, &tv);
    if(ret < 0)
    {
      error = getSocketErrorString();
      return false;
    }
    if(ret == 0)
    {
      received = 0;
      if(size)
        *data = 0;
      return true;
    };

    CURLcode status = curl_easy_recv(curl, data, size, &received);
    if(status != CURLE_OK)
    {
      error.printf("%s.", curl_easy_strerror(status));
      return false;
    }
    if(received == 0)
    {
      error = "Connection was closed.";
      return false;
    }
    return true;
  }
}

String Websocket::getSocketErrorString()
{
  int_t error = ERRNO;
#ifdef _WIN32
  char errorMessage[256];
  DWORD len = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) errorMessage,
        256, NULL );
  ASSERT(len >= 0 && len <= 256);
  while(len > 0 && isspace(((unsigned char*)errorMessage)[len - 1]))
    --len;
  errorMessage[len] = '\0';
  return String(errorMessage, len);
#else
  const char* errorMessage = ::strerror(error);
  return String(errorMessage, String::length(errorMessage));
#endif
}
