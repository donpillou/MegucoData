
#include <nstd/Console.h>

#include "ServerHandler.h"
#include "ClientHandler.h"
#include "SinkClient.h"

ServerHandler::~ServerHandler()
{
  for(HashMap<uint64_t, ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    delete *i;
  for(HashMap<String, Channel*>::Iterator i = channels.begin(), end = channels.end(); i != end; ++i)
    delete *i;
  for(List<SinkClient*>::Iterator i = sinkClients.begin(), end = sinkClients.end(); i != end; ++i)
    delete *i;
}

Channel* ServerHandler::createChannel(const String& name)
{
  uint64_t channelId = nextChannelId++;
  Channel* channel = new Channel(channelId);
  channels.append(name, channel);
  SinkClient* sinkClient = new SinkClient(name, port);
  if(!sinkClient->start())
  {
    Console::errorf("error: Could not start sink thread for channel %s.\n", (const char_t*)name);
    channels.removeBack();
    delete sinkClient;
    return 0;
  }
  return channel;
}

Channel* ServerHandler::findChannel(const String& name)
{
  HashMap<String, Channel*>::Iterator it = channels.find(name);
  if(it == channels.end())
    return 0;
  return *it;
}

ClientHandler* ServerHandler::findClient(uint64_t id)
{
  HashMap<uint64_t, ClientHandler*>::Iterator it = clients.find(id);
  if(it == clients.end())
    return 0;
  return *it;
}

void_t ServerHandler::acceptedClient(Server::Client& client, uint32_t addr, uint16_t port)
{
  uint64_t clientId = nextClientId++;
  ClientHandler* clientHandler = new ClientHandler(clientId, addr, *this, client);
  client.setListener(clientHandler);
  clients.append(clientId, clientHandler);
}

void_t ServerHandler::closedClient(Server::Client& client)
{
  ClientHandler* clientHandler = (ClientHandler*)client.getListener();
  clients.remove(clientHandler->getId());
  delete clientHandler;
}
