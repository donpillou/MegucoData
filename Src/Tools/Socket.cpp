
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
#include <poll.h>
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

#ifdef _WIN32
  s = socket(AF_INET, SOCK_STREAM, 0);
#else
  s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
#endif
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

void_t Socket::swap(Socket& other)
{
  SOCKET tmp = s;
  s = other.s;
  other.s = tmp;
}

bool_t Socket::accept(Socket& to, uint32_t& ip, uint16_t& port)
{
  if((SOCKET)to.s != INVALID_SOCKET)
    return false;

  struct sockaddr_in sin;
  socklen_t val = sizeof(sin);
#ifdef _WIN32
  to.s = ::accept((SOCKET)s, (struct sockaddr *)&sin, &val);
#else
  to.s = ::accept4((SOCKET)s, (struct sockaddr *)&sin, &val, SOCK_CLOEXEC);
#endif
  if((SOCKET)to.s == INVALID_SOCKET)
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

bool_t Socket::setSendBufferSize(int_t size)
{
  if(setsockopt((SOCKET)s, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size)) != 0)
    return false;
  return true;
}

bool_t Socket::setReceiveBufferSize(int_t size)
{
  if(setsockopt((SOCKET)s, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(size)) != 0)
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

bool_t Socket::getSockName(uint32_t& ip, uint16_t& port)
{
  sockaddr_in addr;
  socklen_t len = sizeof(addr);
  if(getsockname((SOCKET)s, (sockaddr*)&addr, &len) != 0)
    return false;
  ip = ntohl(addr.sin_addr.s_addr);
  port = ntohs(addr.sin_port);
  return true;
}

bool_t Socket::getPeerName(uint32_t& ip, uint16_t& port)
{
  sockaddr_in addr;
  socklen_t len = sizeof(addr);
  if(getpeername((SOCKET)s, (sockaddr*)&addr, &len) != 0)
    return false;
  ip = ntohl(addr.sin_addr.s_addr);
  port = ntohs(addr.sin_port);
  return true;
}

ssize_t Socket::send(const byte_t* data, size_t size)
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

ssize_t Socket::recv(byte_t* data, size_t maxSize, size_t minSize)
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
  if((size_t)r >= minSize)
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

void_t Socket::setLastError(int_t error)
{
#ifdef _WIN32
  WSASetLastError(error);
#else
  errno = error;
#endif
}

int_t Socket::getLastError()
{
  return ERRNO;
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

uint32_t Socket::inetAddr(const String& addr)
{
  return ntohl(inet_addr((const char_t*)addr));
}

String Socket::inetNtoA(uint32_t ip)
{
  in_addr in;
  in.s_addr = htonl(ip);
  char* buf = inet_ntoa(in);
  return String(buf, String::length(buf));
}

struct SocketSelectorPrivate
{
#ifdef _WIN32
  struct SocketInfo
  {
    Socket* socket;
    SOCKET s;
    WSAEVENT event;
    uint_t events;
  };
  Array<WSAEVENT> events;
  HashMap<Socket*, SocketInfo> sockets;
  HashMap<WSAEVENT, SocketInfo*> eventToSocket;

  static long mapEvents(uint_t events)
  {
    long networkEvents = 0;
    if(events & Socket::Selector::readEvent)
      networkEvents |= FD_READ | FD_CLOSE;
    if(events & Socket::Selector::writeEvent)
      networkEvents |= FD_WRITE | FD_CLOSE;
    if(events & Socket::Selector::acceptEvent)
      networkEvents |= FD_ACCEPT;
    if(events & Socket::Selector::connectEvent)
      networkEvents |= FD_CONNECT | FD_CLOSE;
    return networkEvents;
  }
#else
  struct SocketInfo
  {
    Socket* socket;
    size_t index;
    uint_t events;
  };

  Array<pollfd> pollfds;
  HashMap<Socket*, SocketInfo> sockets;
  HashMap<SOCKET, SocketInfo*> fdToSocket;
  HashMap<Socket*, uint_t> selectedSockets;

  static short mapEvents(uint_t events)
  {
    short pollEvents = 0;
    if(events & (Socket::Selector::readEvent | Socket::Selector::acceptEvent))
      pollEvents |= POLLIN | POLLRDHUP | POLLHUP;
    if(events & (Socket::Selector::writeEvent | Socket::Selector::connectEvent))
      pollEvents |= POLLOUT | POLLRDHUP | POLLHUP;
    return pollEvents;
  }
#endif
};

Socket::Selector::Selector() : data(new SocketSelectorPrivate) {}

Socket::Selector::~Selector() {delete (SocketSelectorPrivate*)data;}

void_t Socket::Selector::set(Socket& socket, uint_t events)
{
  SocketSelectorPrivate* p = (SocketSelectorPrivate*)data;
  HashMap<Socket*, SocketSelectorPrivate::SocketInfo>::Iterator it = p->sockets.find(&socket);
#ifdef _WIN32
  if(it != p->sockets.end())
  {
    SocketSelectorPrivate::SocketInfo& sockInfo = *it;
    if(sockInfo.events == events)
      return;
    long networkEvents = SocketSelectorPrivate::mapEvents(events);
    VERIFY(WSAEventSelect(sockInfo.s, sockInfo.event, networkEvents) !=  SOCKET_ERROR);
    sockInfo.events = events;
  }
  else
  {
    SOCKET s = socket.s;
    if(s == INVALID_SOCKET)
      return;
    SocketSelectorPrivate::SocketInfo& sockInfo = p->sockets.append(&socket, SocketSelectorPrivate::SocketInfo());
    sockInfo.socket = &socket;
    sockInfo.s = s;
    WSAEVENT event = WSACreateEvent();
    ASSERT(event != WSA_INVALID_EVENT);
    sockInfo.event = event;
    long networkEvents = SocketSelectorPrivate::mapEvents(events);
    VERIFY(WSAEventSelect(s, event, networkEvents) !=  SOCKET_ERROR);
    sockInfo.events = events;
    p->events.append(event);
    p->eventToSocket.append(event, &sockInfo);
  }
#else
  if(it != p->sockets.end())
  {
    SocketSelectorPrivate::SocketInfo& sockInfo = *it;
    if(sockInfo.events == events)
      return;
    p->pollfds[sockInfo.index].events = SocketSelectorPrivate::mapEvents(events);
    sockInfo.events = events;
  }
  else
  {
    SOCKET s = socket.s;
    if(s == INVALID_SOCKET)
      return;
    SocketSelectorPrivate::SocketInfo& sockInfo = p->sockets.append(&socket, SocketSelectorPrivate::SocketInfo());
    sockInfo.socket = &socket;
    sockInfo.index = p->pollfds.size();
    sockInfo.events = events;
    p->fdToSocket.append(s, &sockInfo);
    pollfd& pfd = p->pollfds.append(pollfd());
    pfd.fd = socket.s;
    pfd.events = SocketSelectorPrivate::mapEvents(events);
    pfd.revents = 0;
  }
#endif
}

void_t Socket::Selector::remove(Socket& socket)
{
  SocketSelectorPrivate* p = (SocketSelectorPrivate*)data;
  HashMap<Socket*, SocketSelectorPrivate::SocketInfo>::Iterator it = p->sockets.find(&socket);
  if(it == p->sockets.end())
    return;
  SocketSelectorPrivate::SocketInfo& sockInfo = *it;
#ifdef _WIN32
  WSAEVENT event = sockInfo.event;
  Array<WSAEVENT>::Iterator itEvent = p->events.find(event);
  if(itEvent != p->events.end())
    p->events.remove(itEvent);
  WSACloseEvent(event);
  p->sockets.remove(it);
  p->eventToSocket.remove(event);
#else
  size_t index = sockInfo.index;
  pollfd& pfd = p->pollfds[index];
  p->fdToSocket.remove(pfd.fd);
  p->pollfds.remove(index);
  p->sockets.remove(it);
  for(HashMap<Socket*, SocketSelectorPrivate::SocketInfo>::Iterator i = p->sockets.begin(), end = p->sockets.end(); i != end; ++i)
  {
    SocketSelectorPrivate::SocketInfo& sockInfo = *i;
    if(sockInfo.index > index)
      --sockInfo.index;
  }
  p->selectedSockets.remove(&socket);
#endif
}

bool_t Socket::Selector::select(Socket*& socket, uint_t& events, timestamp_t timeout)
{
  SocketSelectorPrivate* p = (SocketSelectorPrivate*)data;
#ifdef _WIN32
  DWORD count = (DWORD)p->events.size();
  DWORD dw = WSAWaitForMultipleEvents(count, (WSAEVENT*)p->events, FALSE, (DWORD)timeout, FALSE);
  if(dw >= WSA_WAIT_EVENT_0 && dw < WSA_WAIT_EVENT_0 + count)
  {
    WSAEVENT event = p->events[dw - WSA_WAIT_EVENT_0];
    HashMap<WSAEVENT, SocketSelectorPrivate::SocketInfo*>::Iterator it = p->eventToSocket.find(event);
    ASSERT(it != p->eventToSocket.end());
    SocketSelectorPrivate::SocketInfo* sockInfo = *it;
    WSANETWORKEVENTS selectedEvents = {};
    VERIFY(WSAEnumNetworkEvents(sockInfo->s, event, &selectedEvents) != SOCKET_ERROR);
    uint32_t revents = selectedEvents.lNetworkEvents;

    events = 0;
    if(revents & (FD_READ | FD_CLOSE | FD_ACCEPT))
      events |= sockInfo->events & (readEvent | acceptEvent);
    if(revents & (FD_WRITE | FD_CONNECT) || (events == 0 && revents & FD_CLOSE))
      events |= sockInfo->events & (writeEvent | connectEvent);

    socket = sockInfo->socket;
    return true;
  }
  else
  {
    socket = 0;
    events = 0;
    return true; // timeout
  }
#else
  if(p->selectedSockets.isEmpty())
  {
    int count = poll(p->pollfds, p->pollfds.size(), timeout);
    if(count > 0)
    {
      for(pollfd* i = p->pollfds, * end = i + p->pollfds.size(); i < end; ++i)
      {
        if(i->revents)
        {
          HashMap<SOCKET, SocketSelectorPrivate::SocketInfo*>::Iterator it = p->fdToSocket.find(i->fd);
          ASSERT(it != p->fdToSocket.end());
          SocketSelectorPrivate::SocketInfo* sockInfo = *it;
          uint_t events = 0;
          uint_t revents = i->revents;

          if(revents & (POLLIN | POLLRDHUP | POLLHUP))
            events |= sockInfo->events & (readEvent | acceptEvent);
          if(revents & POLLOUT || (events == 0 && revents & (POLLRDHUP | POLLHUP)))
            events |= sockInfo->events & (writeEvent | connectEvent);

          p->selectedSockets.append(sockInfo->socket, events);

          i->revents = 0;
          if(--count == 0)
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
#endif
}
