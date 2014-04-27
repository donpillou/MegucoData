
#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <cstring>
#endif

#ifdef _WIN32
#define ERRNO WSAGetLastError()
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EINPROGRESS WSAEINPROGRESS
#define CLOSE closesocket
typedef int socklen_t;
#define MSG_NOSIGNAL 0
#else
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define ERRNO errno
#define CLOSE close
#define SOCKET_ERROR (-1)
#endif

#include "Socket.h"
#include <nstd/Debug.h>
#include <nstd/HashMap.h>
#include <nstd/Array.h>

#ifdef _WIN32
static class SocketFramework
{
public:
  SocketFramework()
  {
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(wVersionRequested, &wsaData);
  }
  ~SocketFramework()
  {
    WSACleanup();
  }
} socketFramework;
#endif

Socket::Socket() : s(INVALID_SOCKET) {}

Socket::~Socket()
{
  if((SOCKET)s != INVALID_SOCKET)
    ::CLOSE((SOCKET)s);
}

bool_t Socket::open()
{
  if((SOCKET)s != INVALID_SOCKET)
    return false;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if((SOCKET)s == INVALID_SOCKET)
    return false;
  return true;
}

bool_t Socket::close()
{
  if((SOCKET)s == INVALID_SOCKET)
    return false;
  ::CLOSE((SOCKET)s);
  s = INVALID_SOCKET;
  return true;
}

bool_t Socket::isOpen() const
{
  return (SOCKET)s != INVALID_SOCKET;
}

bool_t Socket::accept(const Socket& from, uint32_t& ip, uint16_t& port)
{
  if((SOCKET)s != INVALID_SOCKET)
    return false;

  struct sockaddr_in sin;
  socklen_t val = sizeof(sin);
  s = ::accept((SOCKET)from.s, (struct sockaddr *)&sin, &val);
  if((SOCKET)s == INVALID_SOCKET)
    return false;
  port = ntohs(sin.sin_port);
  ip = ntohl(sin.sin_addr.s_addr);
  return true;
}

bool_t Socket::setNonBlocking()
{
#ifdef _WIN32
  u_long val = 1;
  if(ioctlsocket((SOCKET)s, FIONBIO, &val))
#else
  if(fcntl((SOCKET)s, F_SETFL, O_NONBLOCK))
#endif
    return false;
  return true;
}

bool_t Socket::setNoDelay()
{
  int val = 1;
  if(setsockopt((SOCKET)s, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(val)) != 0)
    return false;
  return true;
}

bool_t Socket::setKeepAlive()
{
  int val = 1;
  if(setsockopt((SOCKET)s, SOL_SOCKET, SO_KEEPALIVE, (char*)&val, sizeof(val)) != 0)
    return false;
  return true;
}

bool_t Socket::setReuseAddress()
{
  int val = 1;
  if(setsockopt((SOCKET)s, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(val)) != 0)
    return false;
  return true;
}

bool_t Socket::bind(unsigned int ip, unsigned short port)
{
  struct sockaddr_in sin;
  memset(&sin,0,sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = htonl((ip == INADDR_NONE) ? INADDR_ANY : ip);

  if(sin.sin_addr.s_addr == htonl(INADDR_ANY) && port == 0)
    return true;

  if(::bind((SOCKET)s,(struct sockaddr*)&sin,sizeof(sin)) < 0)
    return false;
  return true;
}

bool_t Socket::listen()
{
  if(::listen((SOCKET)s, SOMAXCONN) < 0)
    return false;
  return true;
}

bool_t Socket::connect(unsigned int ip, unsigned short port)
{
  struct sockaddr_in sin;

  memset(&sin,0,sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = htonl(ip);

  if(::connect((SOCKET)s, (struct sockaddr*)&sin, sizeof(sin)) < 0)
  {
    if(ERRNO && ERRNO != EINPROGRESS
#ifdef _WIN32
      && ERRNO != EWOULDBLOCK
#endif
      )
      return false;
  }

  return true;
}

int_t Socket::getAndResetErrorStatus()
{
  if(!s)
    return ERRNO;
  int optVal;
  socklen_t optLen = sizeof(int);
  if(getsockopt((SOCKET)s, SOL_SOCKET, SO_ERROR, (char*)&optVal, &optLen) != 0)
    return ERRNO;
  return optVal;
}

ssize_t Socket::send2(const byte_t* data, size_t size)
{
  int r = ::send((SOCKET)s, (const char*)data, size, MSG_NOSIGNAL);
  if(r == SOCKET_ERROR)
  {
    if(ERRNO == EWOULDBLOCK 
#ifndef _WIN32
      || ERRNO == EAGAIN
#endif
      )
    {
#ifdef _WIN32
      WSASetLastError(0);
#else
      errno = 0;
#endif
    }
    return -1;
  }
  return r;
}

ssize_t Socket::recv2(byte_t* data, size_t maxSize, size_t minSize)
{
  int r = ::recv((SOCKET)s, (char*)data, maxSize, 0);
  switch(r)
  {
  case SOCKET_ERROR:
    if(ERRNO == EWOULDBLOCK 
#ifndef _WIN32
      || ERRNO == EAGAIN
#endif
      )
    {
#ifdef _WIN32
      WSASetLastError(0);
#else
      errno = 0;
#endif
    }
    return -1;
  case 0:
    return 0;
  }
  if(r >= minSize)
    return r;
  size_t received = r;
  for(;;)
  {
    r = ::recv((SOCKET)s, (char*)data + received, maxSize - received, 0);
    switch(r)
    {
    case SOCKET_ERROR:
      if(ERRNO == EWOULDBLOCK 
  #ifndef _WIN32
        || ERRNO == EAGAIN
  #endif
        )
      {
  #ifdef _WIN32
        WSASetLastError(0);
  #else
        errno = 0;
  #endif
        return received;
      }
      return -1;
    case 0:
      return 0;
    }
    received += r;
    if(received >= minSize)
      return received;
  }
}

//bool_t Socket::send(const byte_t* data, size_t size, size_t& sent)
//{
//  int r = ::send((SOCKET)s, (const char*)data, size, MSG_NOSIGNAL);
//  if(r == SOCKET_ERROR)
//  {
//    if(ERRNO == EWOULDBLOCK 
//#ifndef _WIN32
//      || ERRNO == EAGAIN
//#endif
//      )
//    {
//      sent = 0;
//      return true;
//    }
//    return false;
//  }
//  sent = r;
//  return true;
//}
//
//bool_t Socket::recv(byte_t* data, size_t maxSize, size_t& received)
//{
//  int r = ::recv((SOCKET)s, (char*)data, maxSize, 0);
//  switch(r)
//  {
//  case SOCKET_ERROR:
//    if(ERRNO == EWOULDBLOCK 
//#ifndef _WIN32
//      || ERRNO == EAGAIN
//#endif
//      )
//    {
//      received = 0;
//      return true;
//    }
//    return false;
//  case 0:
//    received = 0;
//    return false;
//  }
//  received = r;
//  return true;
//}
//
//bool_t Socket::send(const byte_t* data, size_t size)
//{
//  size_t sent, totalSent = 0;
//  while(totalSent < size)
//  {
//    if(!send(data, size, sent))
//      return false;
//    totalSent += sent;
//  }
//  return true;
//}
//
//bool_t Socket::recv(byte_t* data, size_t size)
//{
//  size_t received, totalReceived = 0;
//  while(totalReceived < size)
//  {
//    if(!recv(data, size, received))
//      return false;
//    totalReceived += received;
//  }
//  return true;
//}

int_t Socket::getLastError()
{
  return ERRNO;
}

String Socket::getLastErrorString()
{
  return getErrorString(ERRNO);
}

String Socket::getErrorString(int_t error)
{
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

struct SocketSelectorPrivate
{
  fd_set readfds;
  fd_set writefds;
#ifndef _WIN32
  int nfds;
#endif
  struct SocketInfo
  {
    Socket* socket;
    uint_t events;
  };
  HashMap<SOCKET, SocketInfo> socketInfo;
  Array<SOCKET> sockets;
  HashMap<Socket*, uint_t> selectedSockets;
};

Socket::Selector::Selector()
{
  SocketSelectorPrivate* p;
  data = p = new SocketSelectorPrivate;
  FD_ZERO(&p->readfds);
  FD_ZERO(&p->writefds);
#ifndef _WIN32
  p->nfds = 0;
#endif
}

Socket::Selector::~Selector() {delete (SocketSelectorPrivate*)data;}

void_t Socket::Selector::set(Socket& socket, uint_t events)
{
  SOCKET s = (SOCKET)socket.s;
  if(s == INVALID_SOCKET)
    return;
  SocketSelectorPrivate* p = (SocketSelectorPrivate*)data;
  HashMap<SOCKET, SocketSelectorPrivate::SocketInfo>::Iterator it = p->socketInfo.find(s);
  if(it != p->socketInfo.end())
  {
    SocketSelectorPrivate::SocketInfo& socketInfo = *it;
    uint_t changedEvents = events ^ socketInfo.events;
    if(changedEvents & Socket::Selector::readEvent)
    {
      if(events & Socket::Selector::readEvent)
        FD_SET(s, &p->readfds);
      else
        FD_CLR(s, &p->readfds);
    }
    if(changedEvents & Socket::Selector::writeEvent)
    {
      if(events & Socket::Selector::writeEvent)
        FD_SET(s, &p->writefds);
      else
        FD_CLR(s, &p->writefds);
    }
    socketInfo.events = events;
  }
  else
  {
    if(events & Socket::Selector::readEvent)
      FD_SET(s, &p->readfds);
    if(events & Socket::Selector::writeEvent)
      FD_SET(s, &p->writefds);
    SocketSelectorPrivate::SocketInfo socketInfo = { &socket, events };
    p->socketInfo.append(s, socketInfo);
    p->sockets.append(s);
#ifndef _WIN32
    if(s >= p->nfds)
      p->nfds = s + 1;
#endif
  }
}

void_t Socket::Selector::remove(Socket& socket)
{
  SocketSelectorPrivate* p = (SocketSelectorPrivate*)data;
  SOCKET s = (SOCKET)socket.s;
  HashMap<SOCKET, SocketSelectorPrivate::SocketInfo>::Iterator it = p->socketInfo.find(s);
  if(it == p->socketInfo.end())
    return;
  uint_t events = (*it).events;
  if(events & Socket::Selector::readEvent)
    FD_CLR((SOCKET)socket.s, &p->readfds);
  if(events & Socket::Selector::writeEvent)
    FD_CLR((SOCKET)socket.s, &p->writefds);
  p->socketInfo.remove(it);
  p->selectedSockets.remove(&socket);
  for(SOCKET* i = p->sockets, * end = i + p->sockets.size(); i < end; ++i)
    if(*i == s)
    {
      p->sockets.remove(i - p->sockets);
      break;
    }
#ifndef _WIN32
  if(s + 1 == p->nfds)
  {
    SOCKET nfds = 0;
    for(SOCKET* i = p->sockets, * end = i + p->sockets.size(); i < end; ++i)
      if(*i > nfds)
        nfds = *i;
    p->nfds = nfds + 1;
  }
#endif
}

//#include <nstd/Console.h>
bool_t Socket::Selector::select(Socket*& socket, uint_t& events, timestamp_t timeout)
{
  SocketSelectorPrivate* p = (SocketSelectorPrivate*)data;

  if(p->selectedSockets.isEmpty())
  {
    fd_set readfds = p->readfds;
    fd_set writefds = p->writefds;
    timeval tv = { (long)(timeout / 1000L), (long)((timeout % 1000LL)) * 1000L };
    //Console::printf("select: readfds=%u, writefds=%u\n", readfds.fd_count, writefds.fd_count);
    int selectionCount =  ::select(
#ifdef _WIN32
      0
#else
      p->nfds
#endif
      , &readfds, &writefds, 0, &tv);
    if(selectionCount == SOCKET_ERROR)
      return false;
    if(selectionCount > 0)
    {
      uint_t events = 0;
      for(SOCKET* i = p->sockets, * end = i + p->sockets.size(); i < end; ++i)
      {
        if(FD_ISSET(*i, &readfds))
        {
          events |= Socket::Selector::readEvent;
          --selectionCount;
        }
        if(FD_ISSET(*i, &writefds))
        {
          events |= Socket::Selector::writeEvent;
          --selectionCount;
        }
        if(events)
        {
          SocketSelectorPrivate::SocketInfo& socketInfo = *p->socketInfo.find(*i);
          p->selectedSockets.append(socketInfo.socket, events);
          events = 0;
          if(selectionCount == 0)
            break;
        }
      }
    }
    if(p->selectedSockets.isEmpty())
    {
      socket = 0;
      events = 0;
      return true; // timeout
    }
  }

  HashMap<Socket*, uint_t>::Iterator it = p->selectedSockets.begin();
  socket = it.key();
  events = *it;
  p->selectedSockets.remove(it);
  return true;
}
