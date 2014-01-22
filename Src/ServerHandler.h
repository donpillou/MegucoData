
#pragma once

#include <nstd/HashMap.h>
#include <nstd/List.h>

#include "Tools/Server.h"
#include "Channel.h"

class ClientHandler;
class SinkClient;

class ServerHandler : public Server::Listener
{
public:
  Channel* createChannel(const String& name);
  Channel* findChannel(const String& name);
  ClientHandler* findClient(uint64_t id);
  const HashMap<String, Channel*>& getChannels() const {return channels;}

  ServerHandler(uint16_t port) : port(port), nextClientId(1), nextChannelId(1) {}
  ~ServerHandler();

private:
  uint16_t port;
  HashMap<String, Channel*> channels;
  HashMap<uint64_t, ClientHandler*> clients;
  uint64_t nextClientId;
  uint64_t nextChannelId;
  List<SinkClient*> sinkClients;

  virtual void_t acceptedClient(Server::Client& client, uint32_t addr, uint16_t port);
  virtual void_t closedClient(Server::Client& client);
};
