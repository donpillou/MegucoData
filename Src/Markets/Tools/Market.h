
#pragma once

#include <nstd/String.h>

class Market
{
public:
  class Trade;

  class Callback
  {
  public:
    virtual bool_t receivedTrade(const Trade& trade) = 0;
    virtual bool_t receivedTime(uint64_t time) = 0;
  };

  class Trade
  {
  public:
    uint64_t id;
    uint64_t time;
    double price;
    double amount;
    uint32_t flags;

  };

  virtual ~Market() {};

  virtual String getChannelName() const = 0;
  virtual bool_t connect() = 0;
  virtual void_t close() = 0;
  virtual bool_t isOpen() const = 0;
  virtual const String& getErrorString() const = 0;
  virtual bool_t process(Callback& callback) = 0;
};
