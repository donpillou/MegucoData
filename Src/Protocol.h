
#pragma once

#include <nstd/Base.h>

class Protocol
{
public:
  enum MessageType
  {
    subscribeRequest,
    subscribeResponse,
    tradeRequest,
    tradeResponse,
    tradeErrorResponse,
    registerSourceRequest,
    registerSourceResponse,
    registerSinkRequest,
    registerSinkResponse,
    registeredSinkMessage,
    tradeMessage,
  };

#pragma pack(push, 1)
  struct Header
  {
    uint32_t size; // including header
    uint64_t source; // client id
    uint64_t destination; // client id
    uint16_t messageType; // MessageType
  };

  struct SubscribeRequest
  {
    char_t channel[33];
    uint64_t maxAge;
  };
  struct SubscribeResponse
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
  struct TradeResponse
  {
    uint64_t channelId;
  };

#pragma pack(pop)

};
