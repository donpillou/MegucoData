
#include <nstd/Thread.h>
#include <nstd/Time.h>
#include <nstd/Console.h>
#include <nstd/Variant.h>

#include "Tools/Json.h"
#include "Tools/HttpRequest.h"
#include "BitstampUsd.h"

bool_t BitstampUsd::connect()
{
  close();

  if(!websocket.connect("ws://ws.pusherapp.com/app/de504dc5763aeef9ff52?protocol=6&client=js&version=2.1.2"))
  {
    error = websocket.getErrorString();
    websocket.close();
    return false;
  }

  if(!websocket.send("{\"event\":\"pusher:subscribe\",\"data\":{\"channel\":\"live_trades\"}}"))
  {
    error = websocket.getErrorString();
    websocket.close();
    return false;
  }
  lastPingTime = Time::time();

  HttpRequest httpRequest;
  Buffer data;
  for(int i = 0;; ++i)
  {
    if(!httpRequest.get("https://www.bitstamp.net/api/ticker/", data))
    {
      error = httpRequest.getErrorString();
      websocket.close();
      return false;
    }
    timestamp_t localTime = Time::time();
    //Console::printf("%s\n", (const byte_t*)data);
    Variant dataVar;
    if(!Json::parse(data, dataVar))
    {
      error = "Could not parse ticker data.";
      websocket.close();
      return false;
    }
    const HashMap<String, Variant>& dataMap = dataVar.toMap();
    timestamp_t serverTime = dataMap.find("timestamp")->toInt64() * 1000LL;  // + up to 8 seconds
    if(i == 0 || serverTime - localTime < localToServerTime)
      localToServerTime = serverTime - localTime;
    if(i == 2)
      break;
    Thread::sleep(2000);
  }

  return true;
}

bool_t BitstampUsd::process(Callback& callback)
{
  if(!callback.receivedTime(toServerTime(Time::time())))
    return false;

  HttpRequest httpRequest;
  {
    Buffer data;
    if(!httpRequest.get("https://www.bitstamp.net/api/transactions/", data))
    {
      error = httpRequest.getErrorString();
      websocket.close();
      return false;
    }

    //Console::printf("%s\n", (const byte_t*)data);
    Variant dataVar;
    if(!Json::parse(data, dataVar))
    {
      error = "Could not parse trade data.";
      websocket.close();
      return false;
    }

    const List<Variant>& dataList = dataVar.toList();
    Market::Trade trade;
    if(!dataList.isEmpty())
      for(List<Variant>::Iterator i = --List<Variant>::Iterator(dataList.end()), begin = dataList.begin();; --i)
      {
        const HashMap<String, Variant>& tradeMap = i->toMap();
        trade.amount =  tradeMap.find("amount")->toDouble();
        trade.price = tradeMap.find("price")->toDouble();
        trade.time = tradeMap.find("date")->toUInt64() * 1000ULL;
        trade.flags = 0;
        trade.id = tradeMap.find("tid")->toUInt64();
        if(!callback.receivedTrade(trade))
            return false;
        if(i == begin)
          break;
      }

    // request ticker data?
    timestamp_t now = Time::time();
    if(now - lastTickerTimer >= 30 * 1000)
    {
      if(httpRequest.get("https://www.bitstamp.net/api/ticker/", data))
      {
        const HashMap<String, Variant>& dataMap = dataVar.toMap();
        Ticker ticker;
        ticker.time = dataMap.find("timestamp")->toInt64() * 1000LL;
        ticker.ask = dataMap.find("ask")->toDouble();
        ticker.bid = dataMap.find("bid")->toDouble();
        if(!callback.receivedTicker(ticker))
            return false;
      }
      lastTickerTimer = now;
    }
  }

  Buffer buffer;
  String bufferStr;
  for(;;)
  {
    // send ping?
    if(Time::time() - lastPingTime > 120 * 1000)
    {
      if(!websocket.sendPing())
      {
        error = websocket.getErrorString();
        websocket.close();
        return false;
      }
      lastPingTime = Time::time();
    }

    // wait for data
    if(!websocket.recv(buffer, 1000))
    {
      error = websocket.getErrorString();
      websocket.close();
      return false;
    }
    if(buffer.isEmpty())
      continue; // timeout
    lastPingTime = Time::time();
    if(!handleStreamData(buffer, callback))
      return false;
  }
  
  return false; // unreachable
}

bool_t BitstampUsd::handleStreamData(const Buffer& data, Callback& callback)
{
  timestamp_t localTime = Time::time();
  //Console::printf("%s\n", (const byte_t*)data);

  Variant dataVar;
  if(Json::parse(data, dataVar))
  {
    const HashMap<String, Variant>& dataMap = dataVar.toMap();
    String event = dataMap.find("event")->toString();
    String channel = dataMap.find("channel")->toString();

    if(channel.isEmpty() && event == "pusher:connection_established")
      return true;
    else if(channel == "live_trades")
    {
      if(event == "pusher_internal:subscription_succeeded")
        return true;
      else if(event == "trade")
      {
        Variant tradeVar;
        if(Json::parse(dataMap.find("data")->toString(), tradeVar))
        {
          const HashMap<String, Variant>& tradeMap = tradeVar.toMap();

          Trade trade;
          trade.id = tradeMap.find("id")->toUInt64();
          trade.time = toServerTime(localTime);
          trade.price = tradeMap.find("price")->toDouble();
          trade.amount = tradeMap.find("amount")->toDouble();
          trade.flags = 0;
          if(!callback.receivedTrade(trade))
            return false;
        }
      }
    }
  }

  return true;
}
