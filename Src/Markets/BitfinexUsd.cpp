
#include <nstd/Thread.h>
#include <nstd/Time.h>
#include <nstd/Console.h>
#include <nstd/Variant.h>

#include "Tools/Json.h"
#include "Tools/Math.h"
#include "Tools/HttpRequest.h"
#include "BitfinexUsd.h"

bool_t BitfinexUsd::connect()
{
  open = true;
  return true;
}

bool_t BitfinexUsd::process(Callback& callback)
{
  if(!callback.receivedTime(Time::time()))
    return false;

  HttpRequest httpRequest;
  Buffer data;
  String dataStr;
  for(;; Thread::sleep(14000))
  {
    String url("https://api.bitfinex.com/v1/trades/btcusd");
    if(lastTimestamp != 0)
      url.printf("https://api.bitfinex.com/v1/trades/btcusd?timestamp=%llu", lastTimestamp);
    if(!httpRequest.get(url, data))
    {
      error = httpRequest.getErrorString();
      open = false;
      return false;
    }

    dataStr.attach((const char_t*)(byte_t*)data, data.size());
    Variant dataVar;
    if(!Json::parse(dataStr, dataVar))
    {
      error = "Could not parse trade data.";
      open = false;
      return false;
    }

    const List<Variant>& tradesList = dataVar.toList();
    Trade trade;
    if(!tradesList.isEmpty())
    {
      if(lastTimestamp == 0)
      {
        const HashMap<String, Variant>& tradeData = tradesList.back().toMap();
        lastTimestamp = tradeData.find("timestamp")->toInt64();
        if(lastTimestamp == 0)
          continue;
        if(!callback.receivedTime(lastTimestamp * 1000LL))
          return false;
      }

      for(List<Variant>::Iterator i = --List<Variant>::Iterator(tradesList.end()), begin = tradesList.begin();; --i)
      {
        const HashMap<String, Variant>& tradeData = i->toMap();
        trade.id = tradeData.find("tid")->toInt64();
        int64_t timestamp = tradeData.find("timestamp")->toInt64();
        trade.time = timestamp * 1000LL;
        trade.price = tradeData.find("price")->toDouble();
        trade.amount = tradeData.find("amount")->toDouble();
        trade.flags = 0;
        if(trade.id > lastTradeId)
        {
          if(!callback.receivedTrade(trade))
            return false;
          lastTradeId = trade.id;
          lastTimestamp = timestamp;
        }
        if(i == begin)
          break;
      }
    }
  }

  return false; // unreachable
}
