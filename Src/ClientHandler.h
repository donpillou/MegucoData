
#pragma once

#include <nstd/HashMap.h>

#include "Tools/Server.h"
#include "Channel.h"
#include "Protocol.h"

class ServerHandler;

class ClientHandler : public Server::Client::Listener, public Channel::Listener
{
public:
  ClientHandler(uint64_t id, uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client) : id(id), clientAddr(clientAddr), serverHandler(serverHandler), client(client), channel(0), mode(clientMode) {}
  ~ClientHandler();

  uint64_t getId() const {return id;}

private:
  virtual size_t handle(byte_t* data, size_t size);

  void_t handleMessage(const Protocol::Header& header, byte_t* data, size_t size);

  void_t sendErrorResponse(Protocol::MessageType messageType, uint64_t destination, uint64_t channelId, const String& errorMessage);

  virtual void_t write();

  virtual void_t addedTrade(Channel& channel, const Protocol::Trade& trade);

  class Subscription
  {
  public:
    Channel* channel;
    uint64_t lastReplayedTradeId;

    Subscription() : lastReplayedTradeId(0) {}
  };

  uint64_t id;
  uint32_t clientAddr;
  ServerHandler& serverHandler;
  Server::Client& client;
  HashMap<uint64_t, Subscription> subscriptions;
  HashSet<Subscription*> pendingRequests;
  Channel* channel; // sink or source channel

  enum Mode
  {
    clientMode,
    sourceMode,
    sinkMode,
  } mode;
};
