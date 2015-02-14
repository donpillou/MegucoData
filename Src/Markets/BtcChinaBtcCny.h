
#pragma once

#include <nstd/List.h>

#include "Tools/Market.h"

class BtcChinaBtcCny : public Market
{
public:
  BtcChinaBtcCny() : open(false), lastTradeId(0) {}

  virtual String getChannelName() const {return String("BtcChina/BTC/CNY");}
  virtual bool_t connect();
  virtual void_t close() {open = false;}
  virtual bool_t isOpen() const {return open;}
  virtual const String& getErrorString() const {return error;}
  virtual bool_t process(Callback& callback);

private:
  String error;
  bool_t open;

  uint64_t lastTradeId;
};
