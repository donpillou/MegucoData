
#include <nstd/Time.h>

#include "ClientHandler.h"
#include "ServerHandler.h"
#include "Protocol.h"

#include "Tools/Math.h"

ClientHandler::~ClientHandler()
{
  for(HashMap<uint64_t, Subscription>::Iterator i = subscriptions.begin(), end = subscriptions.end(); i != end; ++i)
    i->channel->removeListener(*this);

  if(channel)
    switch(mode)
    {
    case clientMode:
      break;
    case sourceMode:
      if(channel->getSourceClient() == this)
        channel->setSourceClient(0);
      break;
    case sinkMode:
      if(channel->getSinkClient() == this)
        channel->setSinkClient(0);
      break;
    }
}

size_t ClientHandler::handle(byte_t* data, size_t size)
{
  byte_t* pos = data;
  while(size > 0)
  {
    if(size < sizeof(Protocol::Header))
      break;
    Protocol::Header* header = (Protocol::Header*)pos;
    if(header->size >= 5000)
    {
      client.close();
      return 0;
    }
    if(size < header->size)
      break;
    if(header->destination && mode == sinkMode)
    {
      ClientHandler* client = serverHandler.findClient(header->destination);
      client->handleMessage(*header, pos + sizeof(Protocol::Header), header->size - sizeof(Protocol::Header));
      pos += header->size;
      size -= header->size;
      continue;
    }
    handleMessage(*header, pos + sizeof(Protocol::Header), header->size - sizeof(Protocol::Header));
    pos += header->size;
    size -= header->size;
  }
  if(size >= 5000)
  {
    client.close();
    return 0;
  }
  return pos - data;
}

void_t ClientHandler::handleMessage(const Protocol::Header& messageHeader, byte_t* data, size_t size)
{
  switch((Protocol::MessageType)messageHeader.messageType)
  {
  case Protocol::registerSourceRequest:
    if(clientAddr == Socket::loopbackAddr && size >= sizeof(Protocol::RegisterSourceRequest) && channel == 0)
    {
      Protocol::RegisterSourceRequest* request = (Protocol::RegisterSourceRequest*)data;
      request->channel[sizeof(request->channel) - 1] = '\0';
      String channelName(request->channel, String::length(request->channel));
      channel = serverHandler.findChannel(channelName);
      if(!channel)
      {
        channel = serverHandler.createChannel(channelName);
        if(!channel)
        {
          String errorMessage;
          errorMessage.printf("Unknown channel %s", (const char_t*)channelName);
          sendErrorResponse((Protocol::MessageType)messageHeader.messageType, messageHeader.source, 0, errorMessage);
          return;
        }
      }
      if(channel->getSourceClient())
        channel->getSourceClient()->client.close();
      channel->setSourceClient(this);
      mode = sourceMode;
      byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::RegisterSourceResponse)];
      Protocol::Header* header = (Protocol::Header*)message;
      Protocol::RegisterSourceResponse* registerSourceResponse = (Protocol::RegisterSourceResponse*)(header + 1);
      header->size = sizeof(message);
      header->destination = messageHeader.source;
      header->source = 0;
      header->messageType = Protocol::registerSourceResponse;
      Memory::copy(registerSourceResponse->channel, (const char_t*)channelName, channelName.length() + 1);
      registerSourceResponse->channelId = channel->getId();
      client.send(message, sizeof(message));
      if(channel->getSinkClient())
      {
        Protocol::Header header;
        header.size = sizeof(header);
        header.destination = header.source = 0;
        header.messageType = Protocol::registeredSinkMessage;
        client.send((byte_t*)&header, sizeof(header));
      }
    }
    break;
  case Protocol::registerSinkRequest:
    if(clientAddr == Socket::loopbackAddr && size >= sizeof(Protocol::RegisterSinkRequest) && channel == 0)
    {
      Protocol::RegisterSinkRequest* request = (Protocol::RegisterSinkRequest*)data;
      request->channel[sizeof(request->channel) - 1] = '\0';
      String channelName(request->channel, String::length(request->channel));
      channel = serverHandler.findChannel(channelName);
      if(!channel)
      {
        channel = serverHandler.createChannel(channelName);
        if(!channel)
        {
          String errorMessage;
          errorMessage.printf("Unknown channel %s", (const char_t*)channelName);
          sendErrorResponse((Protocol::MessageType)messageHeader.messageType, messageHeader.source, 0, errorMessage);
          return;
        }
      }
      if(channel->getSinkClient())
        channel->getSinkClient()->client.close();
      channel->setSinkClient(this);
      mode = sinkMode;
      byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::RegisterSinkResponse)];
      Protocol::Header* header = (Protocol::Header*)message;
      Protocol::RegisterSinkResponse* registerSinkResponse = (Protocol::RegisterSinkResponse*)(header + 1);
      header->size = sizeof(message);
      header->destination = messageHeader.source;
      header->source = 0;
      header->messageType = Protocol::registerSinkResponse;
      Memory::copy(registerSinkResponse->channel, (const char_t*)channelName, channelName.length() + 1);
      registerSinkResponse->channelId = channel->getId();
      client.send(message, sizeof(message));
      if(channel->getSourceClient())
      {
        Protocol::Header header;
        header.size = sizeof(header);
        header.destination = header.source = 0;
        header.messageType = Protocol::registeredSinkMessage;
        channel->getSourceClient()->client.send((byte_t*)&header, sizeof(header));
      }
    }
    break;
  case Protocol::subscribeRequest:
    if(size >= sizeof(Protocol::SubscribeRequest))
    {
      Protocol::SubscribeRequest* request = (Protocol::SubscribeRequest*)data;
      request->channel[sizeof(request->channel) - 1] = '\0';
      String channelName(request->channel, String::length(request->channel));
      Channel* channel = serverHandler.findChannel(channelName);
      if(!channel)
      {
        String errorMessage;
        errorMessage.printf("Unknown channel %s", (const char_t*)channelName);
        sendErrorResponse((Protocol::MessageType)messageHeader.messageType, messageHeader.source, 0, errorMessage);
        return;
      }
      uint64_t channelId = channel->getId();
      if(subscriptions.find(channelId) != subscriptions.end())
        return;
      Subscription& subscription = subscriptions.append(channelId, Subscription());
      subscription.channel = channel;

      byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::SubscribeResponse)];
      Protocol::Header* header = (Protocol::Header*)message;
      Protocol::SubscribeResponse* subscribeResponse = (Protocol::SubscribeResponse*)(header + 1);
      header->size = sizeof(message);
      header->destination = messageHeader.source;
      header->source = 0;
      header->messageType = Protocol::subscribeResponse;
      Memory::copy(subscribeResponse->channel, (const char_t*)channelName, channelName.length() + 1);
      subscribeResponse->channelId = channelId;
      client.send(message, sizeof(message));

      ClientHandler* sinkClient = channel->getSinkClient();
      if(request->maxAge != 0 && sinkClient)
      {
        byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::TradeRequest)];
        Protocol::Header* header = (Protocol::Header*)message;
        Protocol::TradeRequest* tradeRequest = (Protocol::TradeRequest*)(header + 1);
        header->size = sizeof(message);
        header->destination = 0;
        header->source = this->id;
        header->messageType = Protocol::tradeRequest;
        tradeRequest->sinceId = 0;
        tradeRequest->maxAge = request->maxAge;
        sinkClient->client.send(message, sizeof(message));
      }
      else
        channel->addListener(*this);
    }
    break;
  case Protocol::unsubscribeRequest:
    if(size >= sizeof(Protocol::UnsubscribeRequest))
    {
      Protocol::UnsubscribeRequest* request = (Protocol::UnsubscribeRequest*)data;
      request->channel[sizeof(request->channel) - 1] = '\0';
      String channelName(request->channel, String::length(request->channel));
      Channel* channel = serverHandler.findChannel(channelName);
      if(!channel)
      {
        String errorMessage;
        errorMessage.printf("Unknown channel %s", (const char_t*)channelName);
        sendErrorResponse((Protocol::MessageType)messageHeader.messageType, messageHeader.source, 0, errorMessage);
        return;
      }
      uint64_t channelId = channel->getId();
      HashMap<uint64_t, Subscription>::Iterator it = subscriptions.find(channelId);
      if(it == subscriptions.end())
      {
        String errorMessage;
        errorMessage.printf("Not subscribed to %s", (const char_t*)channelName);
        sendErrorResponse((Protocol::MessageType)messageHeader.messageType, messageHeader.source, 0, errorMessage);
        return;
      }
      subscriptions.remove(it);
      channel->removeListener(*this);

      byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::UnsubscribeResponse)];
      Protocol::Header* header = (Protocol::Header*)message;
      Protocol::UnsubscribeResponse* unsubscribeResponse = (Protocol::UnsubscribeResponse*)(header + 1);
      header->size = sizeof(message);
      header->destination = messageHeader.source;
      header->source = 0;
      header->messageType = Protocol::unsubscribeResponse;
      Memory::copy(unsubscribeResponse->channel, (const char_t*)channelName, channelName.length() + 1);
      unsubscribeResponse->channelId = channelId;
      client.send(message, sizeof(message));
    }
    break;
  case Protocol::errorResponse:
    if(size >= sizeof(Protocol::ErrorResponse))
    {
      Protocol::ErrorResponse* errorResponse = (Protocol::ErrorResponse*)data;
      switch(errorResponse->messageType)
      {
      case Protocol::tradeRequest: // stop requesting trade history and start listening to new trades
        {
          HashMap<uint64_t, Subscription>::Iterator it = subscriptions.find(errorResponse->channelId);
          if(it == subscriptions.end())
            return;
          Subscription& subscription = *it;
          subscription.channel->addListener(*this);
        }
        break;
      }
    }
    break;
  case Protocol::tradeResponse:
    if(size >= sizeof(Protocol::TradeResponse))
    {
      Protocol::TradeResponse* tradeResponse = (Protocol::TradeResponse*)data;
      size_t count = (size - sizeof(Protocol::TradeResponse)) / sizeof(Protocol::Trade);
      uint64_t channelId = tradeResponse->channelId;
      HashMap<uint64_t, Subscription>::Iterator it = subscriptions.find(channelId);
      if(it == subscriptions.end())
        return;
      Subscription& subscription = *it;
      byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::TradeMessage)];
      Protocol::Header* header = (Protocol::Header*)message;
      header->size = sizeof(message);
      header->destination = header->source = 0;
      header->messageType = Protocol::tradeMessage;
      uint64_t newestChannelTradeId = subscription.channel->getLastTradeId();
      for(Protocol::Trade* trade = (Protocol::Trade*)(tradeResponse + 1), * tradeEnd = trade + count; trade < tradeEnd; ++trade)
      {
        Protocol::TradeMessage* treadeMessage = (Protocol::TradeMessage*)(header + 1);
        treadeMessage->channelId = channelId;
        treadeMessage->trade.id = trade->id;
        treadeMessage->trade.time = trade->time;
        treadeMessage->trade.price = trade->price;
        treadeMessage->trade.amount = trade->amount;
        treadeMessage->trade.flags = trade->flags | Protocol::TradeFlag::replayedFlag;
        if(trade->id == newestChannelTradeId)
          treadeMessage->trade.flags |= Protocol::TradeFlag::syncFlag;
        client.send(message, sizeof(message));
        subscription.lastReplayedTradeId = trade->id;
      }
      if(subscription.lastReplayedTradeId == newestChannelTradeId)
        subscription.channel->addListener(*this);
      else
        pendingRequests.append(&subscription);
    }
    break;
  case Protocol::tradeMessage:
    if(size >= sizeof(Protocol::TradeMessage))
    {
      Protocol::TradeMessage* tradeMessage = (Protocol::TradeMessage*)data;
      if(channel && channel->getId() == tradeMessage->channelId)
        channel->addTrade(tradeMessage->trade);
    }
    break;
  case Protocol::channelRequest:
    {
      const HashMap<String, Channel*>& channels = serverHandler.getChannels();
      Protocol::Header header;
      Protocol::Channel channelProt;
      header.size = sizeof(header) + channels.size() * sizeof(Protocol::Channel);
      header.destination = messageHeader.source;
      header.source = 0;
      header.messageType = Protocol::channelResponse;
      client.send((byte_t*)&header, sizeof(header));
      for(HashMap<String, Channel*>::Iterator i = channels.begin(), end = channels.end(); i != end; ++i)
      {
        const String& channelName = i.key();
        Memory::copy(&channelProt.channel, (const tchar_t*)channelName, channelName.length() + 1);
        client.send((byte_t*)&channelProt, sizeof(channelProt));
      }
    }
    break;
  case Protocol::timeRequest:
    {
      byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::TimeResponse)];
      Protocol::Header* header = (Protocol::Header*)message;
      Protocol::TimeResponse* timeRespnse = (Protocol::TimeResponse*)(header + 1);
      header->size = sizeof(message);
      header->destination = messageHeader.source;
      header->source = 0;
      header->messageType = Protocol::timeResponse;
      timeRespnse->time = Time::time();
      client.send((byte_t*)message, sizeof(message));
    }
    break;
  case Protocol::timeMessage:
    if(size >= sizeof(Protocol::TimeMessage))
    {
      Protocol::TimeMessage* timeMessage = (Protocol::TimeMessage*)data;
      if(channel && channel->getId() == timeMessage->channelId)
        channel->setServerTime(timeMessage->time);
    }
    break;
  default:
    break;
  }
}

void_t ClientHandler::write() 
{
  if(pendingRequests.isEmpty())
    return;
  Subscription* subscription = pendingRequests.front();
  pendingRequests.remove(pendingRequests.begin());
  ClientHandler* sinkClient = subscription->channel->getSinkClient();
  if(!sinkClient)
    return;
  byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::TradeRequest)];
  Protocol::Header* header = (Protocol::Header*)message;
  Protocol::TradeRequest* tradeRequest = (Protocol::TradeRequest*)(header + 1);
  header->size = sizeof(message);
  header->destination = 0;
  header->source = id;
  header->messageType = Protocol::tradeRequest;
  tradeRequest->sinceId = subscription->lastReplayedTradeId;
  tradeRequest->maxAge = 0;
  sinkClient->client.send(message, sizeof(message));
}

void_t ClientHandler::addedTrade(Channel& channel, const Protocol::Trade& trade)
{
  byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::TradeMessage)];
  Protocol::Header* header = (Protocol::Header*)message;
  Protocol::TradeMessage* treadeMessage = (Protocol::TradeMessage*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = Protocol::tradeMessage;
  treadeMessage->channelId = channel.getId();
  treadeMessage->trade.id = trade.id;
  treadeMessage->trade.time = channel.toLocalTime(trade.time);
  treadeMessage->trade.price = trade.price;
  treadeMessage->trade.amount = trade.amount;
  treadeMessage->trade.flags = trade.flags;
  client.send(message, sizeof(message));
}

void_t ClientHandler::sendErrorResponse(Protocol::MessageType messageType, uint64_t destination, uint64_t channelId, const String& errorMessage)
{
  byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::ErrorResponse)];
  Protocol::Header* header = (Protocol::Header*)message;
  Protocol::ErrorResponse* errorResponse = (Protocol::ErrorResponse*)(header + 1);
  header->size = sizeof(message);
  header->destination = destination;
  header->source = 0;
  header->messageType = Protocol::errorResponse;
  errorResponse->messageType = messageType;
  errorResponse->channelId = channelId;
  Memory::copy(errorResponse->errorMessage, (const char_t*)errorMessage, Math::min(errorMessage.length() + 1, sizeof(errorResponse->errorMessage) - 1));
  errorResponse->errorMessage[sizeof(errorResponse->errorMessage) - 1] = '\0';
  client.send(message, sizeof(message));
}
