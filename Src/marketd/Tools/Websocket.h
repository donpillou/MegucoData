
#pragma once

#include <nstd/String.h>
#include <nstd/Buffer.h>

class Websocket
{
public:
  Websocket(const String& origin = String(), bool useMask = false);
  ~Websocket();

  bool_t connect(const String& url);
  void_t close();
  bool_t isOpen() const;
  bool_t recv(Buffer& buffer, timestamp_t timeout);
  bool_t send(const byte_t* buffer, size_t size);
  bool_t send(const String& data);
  bool_t sendPing();

  const String& getErrorString() const {return error;}

private:
  void_t* curl;
  void_t* s;
  String error;
  String origin;
  bool useMask;
  Buffer recvBuffer;

  bool_t sendAll(const byte_t* data, size_t size);
  bool_t receive(byte_t* data, size_t size, timestamp_t timeout, size_t& received);
  bool_t sendFrame(uint_t type, const byte_t* data, size_t size);

  static String getSocketErrorString();
};
