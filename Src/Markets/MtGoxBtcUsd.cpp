
#include <nstd/Thread.h>
#include <nstd/Time.h>
#include <nstd/Console.h>
#include <nstd/Variant.h>

#include "Tools/Json.h"
#include "Tools/HttpRequest.h"
#include "MtGoxBtcUsd.h"

bool_t MtGoxBtcUsd::connect()
{
  close();

  if(!websocket.connect("ws://websocket.mtgox.com:80/mtgox?Currency=USD"))
  {
    error = websocket.getErrorString();
    websocket.close();
    return false;
  }

  if(!websocket.send("{\"op\": \"mtgox.subscribe\",\"type\": \"ticker\"}") ||
     !websocket.send("{\"op\": \"mtgox.subscribe\",\"type\": \"trades\"}") ||
     !websocket.send("{\"op\": \"mtgox.subscribe\",\"type\": \"depth\"}"))
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
    if(!httpRequest.get("http://data.mtgox.com/api/2/BTCUSD/money/ticker_fast", data))
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
    timestamp_t serverTime = dataMap.find("data")->toMap().find("now")->toInt64() / 1000LL;
    if(i == 0 || serverTime - localTime < localToServerTime)
      localToServerTime = serverTime - localTime;
    if(i == 2)
      break;
    Thread::sleep(2000);
  }

  return true;
}

bool_t MtGoxBtcUsd::process(Callback& callback)
{
  if(!callback.receivedTime(toServerTime(Time::time())))
    return false;

  {
    HttpRequest httpRequest;
    Buffer data;
    if(!httpRequest.get("http://data.mtgox.com/api/2/BTCUSD/money/trades/fetch", data))
    {
      error = httpRequest.getErrorString();
      websocket.close();
      return false;
    }

    Variant dataVar;
    if(!Json::parse(data, dataVar))
    {
      error = "Could not parse trade data.";
      websocket.close();
      return false;
    }

    const List<Variant>& dataList = dataVar.toMap().find("data")->toList();
    Market::Trade trade;
    for(List<Variant>::Iterator i = dataList.begin(), end = dataList.end(); i != end; ++i)
    {
      const HashMap<String, Variant>& tradeMap = i->toMap();
      trade.id = tradeMap.find("tid")->toUInt64();
      trade.amount =  tradeMap.find("amount_int")->toUInt64() / (double)100000000ULL;;
      trade.price = tradeMap.find("price_int")->toUInt64() / (double)100000ULL;;
      trade.time = tradeMap.find("date")->toInt64() * 1000LL;
      trade.flags = 0;
      String itemCurrency = tradeMap.find("item")->toString().toUpperCase();
      String priceCurrency = tradeMap.find("price_currency")->toString().toUpperCase();
      if(itemCurrency == "BTC" && priceCurrency == "USD")
        if(!callback.receivedTrade(trade))
          return false;
    }
  }

  Buffer buffer;
  String bufferStr;
  for(;;)
  {
    // send ping?
    if(Time::time() - lastPingTime > 3 * 60 * 1000)
    {
      if(!sendPing())
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

bool_t MtGoxBtcUsd::sendPing()
{
  // mtgox websocket server does not support websockt pings. hence lets send some valid commands
  if(!websocket.send("{\"op\": \"unsubscribe\",\"channel\": \"d5f06780-30a8-4a48-a2f8-7ed181b4a13f \"}") ||
     !websocket.send("{\"op\": \"mtgox.subscribe\",\"type\": \"ticker\"}"))
    return false;
  return true;
}

bool_t MtGoxBtcUsd::handleStreamData(const Buffer& data, Callback& callback)
{
  Variant dataVar;
  if(Json::parse(data, dataVar))
  {
    const HashMap<String, Variant>& dataMap = dataVar.toMap();
    String channel = dataMap.find("channel")->toString();

    if(channel == "dbf1dee9-4f2e-4a08-8cb7-748919a71b21")
    {
      const HashMap<String, Variant>& tradeMap = dataMap.find("trade")->toMap();

      Trade trade;
      trade.id = tradeMap.find("tid")->toUInt64();
      trade.amount =  tradeMap.find("amount_int")->toUInt64() / (double)100000000ULL;;
      trade.price = tradeMap.find("price_int")->toUInt64() / (double)100000ULL;;
      trade.time = tradeMap.find("date")->toInt64() * 1000LL;
      trade.flags = 0;

      String itemCurrency = tradeMap.find("item")->toString().toUpperCase();
      String priceCurrency = tradeMap.find("price_currency")->toString().toUpperCase();
      if(itemCurrency == "BTC" && priceCurrency == "USD")
      {
        if(!callback.receivedTrade(trade))
          return false;
      }
    }
  }

  return true;
}
