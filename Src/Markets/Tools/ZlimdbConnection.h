
#pragma once

#include <nstd/String.h>

#include "Tools/Market.h"

struct _zlimdb;

class ZlimdbConnection
{
public:
  ZlimdbConnection() : zdb(0), tradesTableId(0), tickerTableId(0) {}
  ~ZlimdbConnection() {close();}

  bool_t connect(const String& channelName);
  void_t close();
  bool_t isOpen() const;
  const String& getErrorString() const {return error;}
  bool_t sendTrade(const Market::Trade& trade);
  bool_t sendTicker(const Market::Ticker& ticker);

private:
  String error;
  _zlimdb* zdb;
  uint32_t tradesTableId;
  uint32_t tickerTableId;
};
