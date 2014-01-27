
#pragma once

#include <nstd/String.h>
#include "Tools/Socket.h"
#include "Tools/Market.h"
#include "Protocol.h"

class RelayConnection
{
public:
  RelayConnection() : channelId(0) {}

  bool_t connect(uint16_t port, const String& channelName);
  void_t close() {socket.close();}
  bool_t isOpen() const {return socket.isOpen();}
  const String& getErrorString() const {return error;}
  uint64_t getChannelId() const {return channelId;}
  bool_t sendTrade(const Market::Trade& trade);
  bool_t sendServerTime(uint64_t time);

private:
  Socket socket;
  String error;
  uint64_t channelId;
};
