
#pragma once

#include <nstd/String.h>
#include <nstd/HashSet.h>

#include "DataProtocol.h"

class ClientHandler;

class Channel
{
public:
  class Listener
  {
  public:
    virtual void_t addedTrade(Channel& channel, const DataProtocol::Trade& trade) = 0;
    virtual void_t addedTicker(Channel& channel, const DataProtocol::TickerMessage& tickerMessage) = 0;
  };

  Channel(uint64_t id);

  uint64_t getId() const {return id;}

  void_t addTrade(const DataProtocol::Trade& trade);
  void_t setServerTime(uint64_t time);
  void_t addTicker(const DataProtocol::TickerMessage& tickerMessage);

  void_t addListener(Listener& listener) {listeners.append(&listener);}
  void_t removeListener(Listener& listener) {listeners.remove(&listener);}
  ClientHandler* getSinkClient() const {return sinkClient;}
  ClientHandler* getSourceClient() const {return sourceClient;}
  void_t setSinkClient(ClientHandler* sinkClient) {this->sinkClient = sinkClient;}
  void_t setSourceClient(ClientHandler* sourceClient) {this->sourceClient = sourceClient;}
  uint64_t getLastTradeId() const {return lastTradeId;}
  const DataProtocol::TickerMessage& getLastTicker() {return lastTickerMessage;}
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
  DataProtocol::TickerMessage lastTickerMessage;
};
