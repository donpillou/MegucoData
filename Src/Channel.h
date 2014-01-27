
#pragma once

#include <nstd/String.h>
#include <nstd/HashSet.h>

#include "Protocol.h"

class ClientHandler;

class Channel
{
public:
  class Listener
  {
  public:
    virtual void_t addedTrade(Channel& channel, const Protocol::Trade& trade) = 0;
  };

  Channel(uint64_t id) : id(id), sinkClient(0), sourceClient(0), lastTradeId(0), lastTradeTime(0), serverToLocalTime(0) {}

  uint64_t getId() const {return id;}

  void_t addTrade(const Protocol::Trade& trade);
  void_t setServerTime(uint64_t time);

  void_t addListener(Listener& listener) {listeners.append(&listener);}
  void_t removeListener(Listener& listener) {listeners.remove(&listener);}
  ClientHandler* getSinkClient() const {return sinkClient;}
  ClientHandler* getSourceClient() const {return sourceClient;}
  void_t setSinkClient(ClientHandler* sinkClient) {this->sinkClient = sinkClient;}
  void_t setSourceClient(ClientHandler* sourceClient) {this->sourceClient = sourceClient;}
  uint64_t getLastTradeId() const {return lastTradeId;}
  timestamp_t toLocalTime(timestamp_t serverTime) const {return serverTime + serverToLocalTime;}

private:
  uint64_t id;
  String name;
  HashSet<Listener*> listeners;
  ClientHandler* sinkClient;
  ClientHandler* sourceClient;
  uint64_t lastTradeId;
  uint64_t lastTradeTime;
  timestamp_t serverToLocalTime;
};
