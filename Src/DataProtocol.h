
#pragma once

#include <nstd/Base.h>

class DataProtocol
{
public:
  enum MessageType
  {
    errorResponse,
    subscribeRequest,
    subscribeResponse,
    unsubscribeRequest,
    unsubscribeResponse,
    tradeRequest,
    tradeResponse,
    registerSourceRequest,
    registerSourceResponse,
    registerSinkRequest,
    registerSinkResponse,
    registeredSinkMessage,
    tradeMessage,
    channelRequest,
    channelResponse,
    timeRequest,
    timeResponse,
    timeMessage,
    tickerMessage,
  };

  enum TradeFlag
  {
    replayedFlag = 0x01,
    syncFlag = 0x02,
  };

  //enum ChannelType
  //{
  //  tradeType,
  //  orderBookType,
  //};
  //
  //enum ChannelFlag
  //{
  //  onlineFlag = 0x01,
  //};

#pragma pack(push, 4)
  struct Header
  {
    uint32_t size; // including header
    uint64_t source; // client id
    uint64_t destination; // client id
    uint16_t messageType; // MessageType
  };

  struct ErrorResponse
  {
    uint16_t messageType;
    uint64_t channelId;
    char_t errorMessage[128];
  };

  struct SubscribeRequest
  {
    char_t channel[33];
    uint64_t maxAge;
    uint64_t sinceId;
  };
  struct SubscribeResponse
  {
    char_t channel[33];
    uint64_t channelId;
    uint32_t flags;
  };
  struct UnsubscribeRequest
  {
    char_t channel[33];
  };
  struct UnsubscribeResponse
  {
    char_t channel[33];
    uint64_t channelId;
  };

  struct RegisterSourceRequest
  {
    char_t channel[33];
  };
  struct RegisterSourceResponse
  {
    char_t channel[33];
    uint64_t channelId;
  };

  struct RegisterSinkRequest
  {
    char_t channel[33];
  };
  struct RegisterSinkResponse
  {
    char_t channel[33];
    uint64_t channelId;
  };

  struct TradeRequest
  {
    uint64_t maxAge;
    uint64_t sinceId;
  };
  struct Trade
  {
    uint64_t id;
    uint64_t time;
    double price;
    double amount;
    uint32_t flags;
  };
  struct TradeMessage
  {
    uint64_t channelId;
    Trade trade;
  };
  struct TimeMessage
  {
    uint64_t channelId;
    uint64_t time;
  };
  struct TradeResponse
  {
    uint64_t channelId;
  };
  struct Channel
  {
    char_t channel[33];
    //uint8_t type;
    //uint32_t flags;
  };

  struct TimeResponse
  {
    uint64_t time;
  };

  struct Ticker
  {
    uint64_t time;
    double bid;
    double ask;
  };

  struct TickerMessage
  {
    uint64_t channelId;
    Ticker ticker;
  };

#pragma pack(pop)

};
