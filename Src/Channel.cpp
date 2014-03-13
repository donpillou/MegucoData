
#include <nstd/Time.h>

#include "Channel.h"

Channel::Channel(uint64_t id) : id(id), sinkClient(0), sourceClient(0), lastTradeId(0), lastTradeTime(0), serverToLocalTime(0)
{
  Memory::zero(&lastTickerMessage, sizeof(lastTickerMessage));
}

void_t Channel::addTrade(const DataProtocol::Trade& trade)
{
  if(trade.id <= lastTradeId)
    return;
  if(trade.time < lastTradeTime)
  { // this can't be true, since the trade id should increase with each trade.
    // let's hope the last trade time was not horribly wrong and shift the current trade time to come after the last one.
    DataProtocol::Trade shiftedTrade = trade;
    shiftedTrade.time = lastTradeTime;
    addTrade(shiftedTrade);
    return;
  }
  for(HashSet<Listener*>::Iterator i = listeners.begin(), end = listeners.end(); i != end; ++i)
    (*i)->addedTrade(*this, trade);
  lastTradeId = trade.id;
  lastTradeTime = trade.time;
}

void_t Channel::addTicker(const DataProtocol::TickerMessage& tickerMessage)
{
  for(HashSet<Listener*>::Iterator i = listeners.begin(), end = listeners.end(); i != end; ++i)
    (*i)->addedTicker(*this, tickerMessage);
  lastTickerMessage = tickerMessage;
}

void_t Channel::setServerTime(uint64_t time)
{
  serverToLocalTime = Time::time() - time;
}
