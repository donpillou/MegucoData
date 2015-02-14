
#include <nstd/Thread.h>
#include <nstd/Time.h>
#include <nstd/Console.h>
#include <nstd/Variant.h>

#include "Tools/Json.h"
#include "Tools/HttpRequest.h"

#include "BtcChinaBtcCny.h"

bool_t BtcChinaBtcCny::connect()
{
  open = true;
  return true;
}

bool_t BtcChinaBtcCny::process(Callback& callback)
{
  HttpRequest httpRequest;
  Buffer data;
  String dataStr;
  for(;; Thread::sleep(14000))
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
    timestamp_t localTime = Time::time();

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
    for(List<Variant>::Iterator i = tradesList.begin(), end = tradesList.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& tradeData = i->toMap();
      trade.id = tradeData.find("tid")->toInt64();
      trade.time = tradeData.find("date")->toInt64() * 1000LL;
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
