
#pragma once

#include <nstd/String.h>

class Socket
{
public:
  Socket();
  ~Socket();

  enum Addr
  {
      anyAddr = 0,
      loopbackAddr = 0x7f000001,
  };

  bool_t open();
  bool_t close();
  bool_t isOpen() const;

  bool_t accept(const Socket& from, uint32_t& ip, uint16_t& port);
  bool_t setKeepAlive();
  bool_t setReuseAddress();
  bool_t setNonBlocking();
  bool_t setNoDelay();
  bool_t bind(uint32_t ip, uint16_t port);
  bool_t listen();
  bool_t connect(uint32_t ip, uint16_t port);

  ssize_t send2(const byte_t* data, size_t size);
  ssize_t recv2(byte_t* data, size_t maxSize, size_t minSize = 0);

  //bool_t send(const byte_t* data, size_t maxSize, size_t& sent);
  //bool_t recv(byte_t* data, size_t maxSize, size_t& received);
  //
  //bool_t send(const byte_t* data, size_t size);
  //bool_t recv(byte_t* data, size_t size);

  int_t getAndResetErrorStatus();

  static int_t getLastError();
  static String getLastErrorString();
  static String getErrorString(int_t error);

  class Selector
  {
  public:
    enum Event
    {
      readEvent = 0x01,
      writeEvent = 0x02,
    };

    Selector();
    ~Selector();

    void_t set(Socket& socket, uint_t events);
    void_t remove(Socket& socket);

    bool_t select(Socket*& socket, uint_t& events, timestamp_t timeout);
  private:
    void_t* data;
  };

private:
#ifdef _WIN32
  int_t s; // TODO: use correct type
#else
  int_t s;
#endif

  Socket(const Socket&);
  Socket& operator=(const Socket&);
};
