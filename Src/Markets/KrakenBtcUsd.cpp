
#include <nstd/Thread.h>
#include <nstd/Time.h>
#include <nstd/Console.h>
#include <nstd/Variant.h>

#include "Tools/Json.h"
#include "Tools/Math.h"
#include "Tools/HttpRequest.h"
#include "KrakenBtcUsd.h"

bool_t KrakenBtcUsd::connect()
{
  open = true;
  return true;
}

bool_t KrakenBtcUsd::process(Callback& callback)
{
  if(!callback.receivedTime(Time::time()))
    return false;

  HttpRequest httpRequest;
  Buffer data;
  String dataStr;

  //request server time
  timestamp_t timeDiff = 0;
  for(int i = 0; i < 3; ++i, Thread::sleep(3000))
  {
    if(!httpRequest.get("https://api.kraken.com/0/public/Time", data))
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
    const HashMap<String, Variant>& dataMap = dataVar.toMap();
    String error = dataMap.find("error")->toString();
    if(!error.isEmpty())
    {
      this->error = error;;
      open = false;
      return false;
    }

    timestamp_t serverTime = dataMap.find("result")->toMap().find("unixtime")->toInt64() * 1000;
    timestamp_t newTimeDiff = serverTime - Time::time();
    if(newTimeDiff < timeDiff || i == 0)
      timeDiff = newTimeDiff;
  }
  if(!callback.receivedTime(Time::time() + timeDiff))
    return false;

  // receive trades
  for(;; Thread::sleep(14000))
  {
    String url("https://api.kraken.com/0/public/Trades?pair=XBTUSD");
    if(lastId != 0)
      url.printf("https://api.kraken.com/0/public/Trades?pair=XBTUSD&since=%llu", lastId);
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
    const HashMap<String, Variant>& dataMap = dataVar.toMap();
    String error = dataMap.find("error")->toString();
    if(!error.isEmpty())
    {
      this->error = error;;
      open = false;
      return false;
    }

    const HashMap<String, Variant>& result = dataMap.find("result")->toMap();
    const List<Variant>& tradesList = result.find("XXBTZUSD")->toList();
    Trade trade;
    for(List<Variant>::Iterator i = tradesList.begin(), end = tradesList.end(); i != end; ++i)
    {
      const List<Variant>& tradeData = i->toList();
      if(tradeData.size() < 3)
        continue;
      List<Variant>::Iterator it = tradeData.begin();
      trade.price = it->toDouble();
      ++it;
      trade.amount = it->toDouble();
      ++it;
      trade.id = (uint64_t)(it->toDouble() * 10000.);
      trade.time = trade.id / 10;
      trade.flags = 0;

      if(trade.id > lastTradeId)
      {
        if(!callback.receivedTrade(trade))
          return false;
        lastTradeId = trade.id;
      }
    }
    lastId = result.find("last")->toUInt64();
  }

  return false; // unreachable
}
