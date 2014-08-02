
#pragma once

#include <nstd/List.h>

#include "Tools/Market.h"

class HuobiBtcCny : public Market
{
public:
  HuobiBtcCny() : open(false), lastTradeId(0) {}

  virtual String getChannelName() const {return String("Huobi BTC/CNY");}
  virtual bool_t connect();
  virtual void_t close() {open = false;}
  virtual bool_t isOpen() const {return open;}
  virtual const String& getErrorString() const {return error;}
  virtual bool_t process(Callback& callback);

private:
  String error;
  bool_t open;

  List<String> lastTradeList;
  uint64_t lastTradeId;
};
