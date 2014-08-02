
#pragma once

#include <nstd/List.h>

#include "Tools/Market.h"

class BtceBtcUsd : public Market
{
public:
  BtceBtcUsd() : open(false), lastTradeId(0), lastTimestamp(0) {}

  virtual String getChannelName() const {return String("Btce BTC/USD");}
  virtual bool_t connect();
  virtual void_t close() {open = false;}
  virtual bool_t isOpen() const {return open;}
  virtual const String& getErrorString() const {return error;}
  virtual bool_t process(Callback& callback);

private:
  String error;
  bool_t open;

  uint64_t lastTradeId;
  int64_t lastTimestamp;

};
