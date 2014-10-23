
#pragma once

#include <nstd/Thread.h>
#include <nstd/String.h>
#include <nstd/HashMap.h>
#include <nstd/File.h>

#include "DataProtocol.h"

class SinkClient
{
public:
  SinkClient(const String& channelName, uint16_t serverPort);

  bool_t start() {return thread.start(main, this);}

private:
  String channelName;
  String dirName;
  uint16_t serverPort;
  Thread thread;

  class Trade
  {
  public:
    uint64_t time;
    double price;
    double amount;
    uint32_t flags;
  };

  uint64_t channelId;
  uint64_t lastTradeId;
  uint64_t lastFileTradeId;
  HashMap<uint64_t, Trade> trades;
  HashMap<uint64_t, uint64_t> keyTrades;
  HashMap<uint64_t, String> tradeFiles;

  String fileDate;
  String fileName;
  File file;

  static uint_t main(void_t* param);

  bool_t handleMessage(Socket& socket, const DataProtocol::Header& messageHeader, byte_t* data, size_t size);
  bool_t handleTradeMessage(DataProtocol::TradeMessage& tradeMessage);

  void_t loadTradesFromFile();
  void_t loadTradesFromFile(const String& file);
  bool_t sendTradesFile(uint64_t source, Socket& socket, const String& file);

  void_t addTrade(const DataProtocol::Trade& trade);
};
