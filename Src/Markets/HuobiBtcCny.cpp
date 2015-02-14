
#include <nstd/Thread.h>
#include <nstd/Time.h>
#include <nstd/Console.h>
#include <nstd/Variant.h>
#include <nstd/Math.h>

#include "Tools/Json.h"
#include "Tools/HttpRequest.h"

#include "HuobiBtcCny.h"

bool_t HuobiBtcCny::connect()
{
  open = true;
  return true;
}

bool_t HuobiBtcCny::process(Callback& callback)
{
  HttpRequest httpRequest;
  Buffer data;
  String dataStr;
  for(;; Thread::sleep(2000))
  {
    
    if(!httpRequest.get("http://market.huobi.com/staticmarket/detail_btc_json.js", data))
    {
      error = httpRequest.getErrorString();
      open = false;
      return false;
    }

    dataStr.attach((const char_t*)(byte_t*)data, data.size());
    //if(dataStr.length() < 13 || !dataStr.startsWith("view_detail("))
    //{
    //  error = "Could not parse trade data.";
    //  open = false;
    //  return false;
    //}
    //dataStr = dataStr.substr(12, dataStr.length() - 12 - 1);

    Variant dataVar;
    if(!Json::parse(dataStr, dataVar))
    {
      error = "Could not parse trade data.";
      open = false;
      return false;
    }

    const List<Variant>& tradesList = dataVar.toMap().find("trades")->toList();
    timestamp_t approxServerTimestamp = Time::time() + 8LL * 60LL * 60LL * 1000LL;
    Time approxServerTime(approxServerTimestamp, true); // Hong Kong time

    Trade trade;
    List<Variant>::Iterator i = tradesList.begin();
    for(List<Variant>::Iterator end = tradesList.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& tradeData = i->toMap();
      String tradeStr = tradeData.find("time")->toString() + " " + tradeData.find("amount")->toString() + " " + tradeData.find("price")->toString() + " " + tradeData.find("type")->toString();
      if(!lastTradeList.isEmpty() && tradeStr == lastTradeList.back())
      {
        List<Variant>::Iterator j = i;
        ++j;
        if(lastTradeList.size() > 1)
          for(List<String>::Iterator x = --List<String>::Iterator(--List<String>::Iterator(lastTradeList.end())), begin = lastTradeList.begin(); j != end; ++j, --x)
          {
            const HashMap<String, Variant>& tradeData = j->toMap();
            String tradeStr = tradeData.find("time")->toString() + " " + tradeData.find("amount")->toString() + " " + tradeData.find("price")->toString() + " " + tradeData.find("type")->toString();
            if(*x != tradeStr)
              goto cont;
            if(x == begin)
              break;
          }
        goto add;
      }
    cont: ;
    }
  add:
    List<Variant>::Iterator begin = tradesList.begin();
    if(i != begin)
      for(--i;; --i)
      {
        const HashMap<String, Variant>& tradeData = i->toMap();
        String tradeStr = tradeData.find("time")->toString() + " " + tradeData.find("amount")->toString() + " " + tradeData.find("price")->toString()+ " " + tradeData.find("type")->toString();
        lastTradeList.append(tradeStr);

        int_t hour, min, sec;
        if(tradeData.find("time")->toString().scanf("%d:%d:%d", &hour, &min, &sec) != 3)
        {
          error = "Could not determine trade timestamp.";
          open = false;
          return false;
        }
        Time tradeTime(approxServerTime);
        tradeTime.hour = hour;
        tradeTime.min = min;
        tradeTime.sec = sec;
        timestamp_t tradeTimestamp = tradeTime.toTimestamp();

        if(Math::abs(tradeTimestamp  - approxServerTimestamp) > 12 * 60 * 60 * 1000LL)
          tradeTimestamp += tradeTimestamp > approxServerTimestamp ? -24 * 60 * 60 * 1000LL : 24 * 60 * 60 * 1000LL;
        if(Math::abs(tradeTimestamp  - approxServerTimestamp) > 60 * 60 * 1000LL)
        {
          error = "Could not determine trade timestamp.";
          open = false;
          return false;
        }

        trade.time = tradeTimestamp - 8 * 60 * 60 * 1000LL;
        trade.id = trade.time;
        if(trade.id == lastTradeId)
          ++trade.id;
        trade.amount = tradeData.find("amount")->toDouble();
        trade.price = tradeData.find("price")->toDouble();
        trade.flags = 0;

        if(!callback.receivedTrade(trade))
          return false;
        lastTradeId = trade.id;

        if(i == begin)
          break;
      }
      while(lastTradeList.size() > 100)
        lastTradeList.removeFront();
    }

  return false; // unreachable
}
