
#pragma once

#include <nstd/Base.h>

class DataProtocol
{
public:
  enum MessageType
  {
    errorResponse,
    loginRequest,
    loginResponse,
    authRequest,
    authResponse,
    addRequest,
    addResponse,
    updateRequest,
    updateResponse,
    removeRequest,
    removeResponse,
    subscribeRequest,
    subscribeResponse,
    unsubscribeRequest,
    unsubscribeResponse,
    queryRequest,
    queryResponse,
  };
  
  enum TableId
  {
    clientsTable,
    tablesTable,
    timeTable,
    usersTable,
  };

#pragma pack(push, 4)
  struct Header
  {
    uint32_t size; // including header
    uint16_t messageType; // MessageType
    uint32_t requestId;
  };

  struct ErrorResponse
  {
    char_t errorMessage[129];
  };

  struct Table
  {
    uint8_t flags; // e.g. PUBLIC, 
  };
--
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
