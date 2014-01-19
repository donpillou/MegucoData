
#pragma once

#include <nstd/String.h>
#include <nstd/HashSet.h>

class ClientHandler;

class Channel
{
public:
  class Trade
  {
  public:
    uint64_t id;
    uint64_t time;
    double price;
    double amount;
    uint_t flags;
  };

  class Listener
  {
  public:
    virtual void_t addedTrade(Channel& channel, const Trade& trade) = 0;
  };

  Channel(uint64_t id) : id(id), sinkClient(0), sourceClient(0), lastTradeId(0), lastTradeTime(0) {}

  uint64_t getId() const {return id;}

  void_t addTrade(const Trade& trade)
  {
    if(trade.id <= lastTradeId)
      return;
    if(trade.time < lastTradeTime)
    { // this can't be true, since the trade id should increase with each trade.
      // let's hope the last trade time was not horribly wrong and shift the current trade time to come after the last one.
      Trade shiftedTrade = trade;
      shiftedTrade.time = lastTradeTime;
      addTrade(shiftedTrade);
      return;
    }
    for(HashSet<Listener*>::Iterator i = listeners.begin(), end = listeners.end(); i != end; ++i)
      (*i)->addedTrade(*this, trade);
    lastTradeId = trade.id;
    lastTradeTime = trade.time;
  }

  void_t addListener(Listener& listener) {listeners.append(&listener);}
  void_t removeListener(Listener& listener) {listeners.remove(&listener);}
  ClientHandler* getSinkClient() const {return sinkClient;}
  ClientHandler* getSourceClient() const {return sourceClient;}
  void_t setSinkClient(ClientHandler* sinkClient) {this->sinkClient = sinkClient;}
  void_t setSourceClient(ClientHandler* sourceClient) {this->sourceClient = sourceClient;}
  uint64_t getLastTradeId() const {return lastTradeId;}

private:
  uint64_t id;
  String name;
  HashSet<Listener*> listeners;
  ClientHandler* sinkClient;
  ClientHandler* sourceClient;
  uint64_t lastTradeId;
  uint64_t lastTradeTime;
};
