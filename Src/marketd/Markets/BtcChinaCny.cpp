
#include <nstd/Thread.h>
#include <nstd/Time.h>
#include <nstd/Console.h>
#include <nstd/Variant.h>

#include "Tools/Json.h"
#include "Tools/Math.h"
#include "Tools/HttpRequest.h"
#include "BtcChinaCny.h"

bool_t BtcChinaCny::connect()
{
  open = true;
  return true;
}

bool_t BtcChinaCny::process(Callback& callback)
{
  if(!callback.receivedTime(Time::time()))
    return false;

  HttpRequest httpRequest;
  Buffer data;
  String dataStr;
  for(;; Thread::sleep(2000))
  {
    String url("https://data.btcchina.com/data/historydata");
    if(lastTradeId != 0)
      url.printf("https://data.btcchina.com/data/historydata?since=%llu", lastTradeId);
    if(!httpRequest.get(url, data))
    {
      error = httpRequest.getErrorString();
      open = false;
      return false;
    }
    timestamp_t localTime =Time::time();

    Variant dataVar;
    if(!Json::parse(dataStr, dataVar))
    {
      error = "Could not parse trade data.";
      open = false;
      return false;
    }

    const List<Variant>& tradesList = dataVar.toList();
    if(!tradesList.isEmpty())
    {
      const HashMap<String, Variant>& tradeData = tradesList.back().toMap();
      timestamp_t serverTime = tradeData.find("date")->toInt64() * 1000LL;
      timestamp_t offset = serverTime - localTime;
      if(offset < timeOffset || !timeOffsetSet)
      {
        timeOffset = offset;
        timeOffsetSet = true;
        if(!callback.receivedTime(serverTime))
          return false;
      }
    }

    Trade trade;
    for(List<Variant>::Iterator i = tradesList.begin(), end = tradesList.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& tradeData = i->toMap();
      trade.id = tradeData.find("tid")->toInt64();
      trade.time = tradeData.find("date")->toInt64();
      trade.price = tradeData.find("price")->toDouble();
      trade.amount = tradeData.find("amount")->toDouble();
      trade.flags = 0;
      if(trade.id > lastTradeId)
      {
        if(!callback.receivedTrade(trade))
          return false;
        lastTradeId = trade.id;
      }
    }
  }

  return false; // unreachable
}
