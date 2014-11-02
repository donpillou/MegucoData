
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

  struct SubscribeRequest
  {
    char_t channel[33];
    uint64_t maxAge;
    uint64_t sinceId;
  };
  struct SubscribeResponse
  {
    uint64_t channelId;
    //uint32_t flags;
  };
  struct UnsubscribeRequest
  {
    uint32_t channelId;
  };
  struct UnsubscribeResponse
  {
    uint32_t channelId;
  };


#pragma pack(pop)

};
