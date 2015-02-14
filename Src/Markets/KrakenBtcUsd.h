
#pragma once

#include <nstd/List.h>

#include "Tools/Market.h"

class KrakenBtcUsd : public Market
{
public:
  KrakenBtcUsd() : open(false), lastId(0), lastTradeId(0) {}

  virtual String getChannelName() const {return String("Kraken/BTC/USD");}
  virtual bool_t connect();
  virtual void_t close() {open = false;}
  virtual bool_t isOpen() const {return open;}
  virtual const String& getErrorString() const {return error;}
  virtual bool_t process(Callback& callback);

private:
  String error;
  bool_t open;

  uint64_t lastId;
  uint64_t lastTradeId;
};
