
#pragma once

#include "Tools/Market.h"
#include "Tools/Websocket.h"

class BitstampUsd : public Market
{
public:
  BitstampUsd() : lastPingTime(0), lastTickerTimer(0) {}

  virtual String getChannelName() const {return String("Bitstamp/USD");}
  virtual bool_t connect();
  virtual void_t close() {websocket.close();}
  virtual bool_t isOpen() const {return websocket.isOpen();}
  virtual const String& getErrorString() const {return error;}
  virtual bool_t process(Callback& callback);

private:
  Websocket websocket;
  String error;
  timestamp_t localToServerTime;
  timestamp_t lastPingTime;
  timestamp_t lastTickerTimer;

  timestamp_t toServerTime(timestamp_t localTime) const {return localTime + localToServerTime;}
  bool_t handleStreamData(const Buffer& data, Callback& callback);
};
