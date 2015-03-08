
#include <nstd/Console.h>
#include <nstd/Thread.h>
#include <nstd/Directory.h>
#include <nstd/Error.h>
#include <nstd/Process.h>

#include "Tools/ZlimdbConnection.h"

#ifdef MARKET_BITSTAMPBTCUSD
#include "BitstampBtcUsd.h"
typedef BitstampBtcUsd MarketConnection;
const char* exchangeName = "BitstampBtcUsd";
#endif
#ifdef MARKET_MTGOXBTCUSD
#include "MtGoxBtcUsd.h"
typedef MtGoxBtcUsd MarketConnection;
const char* exchangeName = "MtGoxBtcUsd";
#endif
#ifdef MARKET_HUOBIBTCCNY
#include "HuobiBtcCny.h"
typedef HuobiBtcCny MarketConnection;
const char* exchangeName = "HuobiBtcCny";
#endif
#ifdef MARKET_BTCCHINABTCCNY
#include "BtcChinaBtcCny.h"
typedef BtcChinaBtcCny MarketConnection;
const char* exchangeName = "BtcChinaBtcCny";
#endif
#ifdef MARKET_BITFINEXBTCUSD
#include "BitfinexBtcUsd.h"
typedef BitfinexBtcUsd MarketConnection;
const char* exchangeName = "BitfinexBtcUsd";
#endif
#ifdef MARKET_BTCEBTCUSD
#include "BtceBtcUsd.h"
typedef BtceBtcUsd MarketConnection;
const char* exchangeName = "BtceBtcUsd";
#endif
#ifdef MARKET_KRAKENBTCUSD
#include "KrakenBtcUsd.h"
typedef KrakenBtcUsd MarketConnection;
const char* exchangeName = "KrakenBtcUsd";
#endif

int_t main(int_t argc, char_t* argv[])
{
  String logFile;

  // parse parameters
  {
    Process::Option options[] = {
        {'b', "daemon", Process::argumentFlag | Process::optionalFlag},
        {'h', "help", Process::optionFlag},
    };
    Process::Arguments arguments(argc, argv, options);
    int_t character;
    String argument;
    while(arguments.read(character, argument))
      switch(character)
      {
      case 'b':
        logFile = argument.isEmpty() ? String(exchangeName, String::length(exchangeName)) + ".log" : argument;
        break;
      case '?':
        Console::errorf("Unknown option: %s.\n", (const char_t*)argument);
        return -1;
      case ':':
        Console::errorf("Option %s required an argument.\n", (const char_t*)argument);
        return -1;
      default:
        Console::errorf("Usage: %s [-b]\n\
  -b, --daemon[=<file>]   Detach from calling shell and write output to <file>.\n", argv[0]);
        return -1;
      }
  }

  // daemonize process
#ifndef _WIN32
  if(!logFile.isEmpty())
  {
    Console::printf("Starting as daemon...\n");
    if(!Process::daemonize(logFile))
    {
      Console::errorf("error: Could not daemonize process: %s\n", (const char_t*)Error::getErrorString());
      return -1;
    }
  }
#endif

  //
  ZlimdbConnection zlimdbConnection;
  MarketConnection marketConnection;
  String channelName = marketConnection.getChannelName();

  class Callback : public Market::Callback
  {
  public:
    virtual bool_t receivedTrade(const Market::Trade& trade)
    {
      if(!zlimdbConnection->sendTrade(trade))
        return false;
      return true;
    }

    virtual bool_t receivedTicker(const Market::Ticker& ticker)
    {
      if(!zlimdbConnection->sendTicker(ticker))
        return false;
      return true;
    }

    ZlimdbConnection* zlimdbConnection;
  } callback;
  callback.zlimdbConnection = &zlimdbConnection;

  for(;; Thread::sleep(10 * 1000))
  {
    if(!zlimdbConnection.isOpen())
    {
      Console::printf("Connecting to zlimdb server...\n");
      if(!zlimdbConnection.connect(channelName))
      {
        Console::errorf("error: Could not connect to zlimdb server: %s\n", (const char_t*)zlimdbConnection.getErrorString());
        continue;
      }
      else
        Console::printf("Connected to zlimdb server.\n");
    }

    if(!marketConnection.isOpen())
    {
      Console::printf("Connecting to %s...\n", (const char_t*)channelName);
      if(!marketConnection.connect())
      {
        Console::errorf("error: Could not connect to %s: %s\n", (const char_t*)channelName, (const char_t*)marketConnection.getErrorString());
        continue;
      }
      else
        Console::printf("Connected to %s.\n", (const char_t*)channelName);
    }

    for(;;)
      if(!marketConnection.process(callback))
        break;

    if(!zlimdbConnection.isOpen())
      Console::errorf("error: Lost connection to zlimdb server: %s\n", (const char_t*)zlimdbConnection.getErrorString());
    if(!marketConnection.isOpen())
      Console::errorf("error: Lost connection to %s: %s\n", (const char_t*)channelName, (const char_t*)marketConnection.getErrorString());
    marketConnection.close(); // reconnect to reload the trade history
  }

  return 0;
}
