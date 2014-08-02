
#pragma once

#include "Tools/Market.h"
#include "Tools/Websocket.h"

class MtGoxBtcUsd : public Market
{
public:
  MtGoxBtcUsd() : websocket("https://github.com/donpillou/traded", true), lastPingTime(0) {}

  virtual String getChannelName() const {return String("MtGox BTC/USD");}
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

  timestamp_t toServerTime(timestamp_t localTime) const {return localTime + localToServerTime;}
  bool_t handleStreamData(const Buffer& data, Callback& callback);
  bool_t sendPing();
};
