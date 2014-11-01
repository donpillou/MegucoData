
#include <nstd/Time.h>

#include "ClientHandler.h"
#include "ServerHandler.h"
#include "DataProtocol.h"

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
    if(size < sizeof(DataProtocol::Header))
      break;
    DataProtocol::Header* header = (DataProtocol::Header*)pos;
    if(header->size < sizeof(DataProtocol::Header) || (header->size >= 5000 && mode != sinkMode))
    {
      client.close();
      return 0;
    }
    if(size < header->size)
      break;
    if(header->destination && mode == sinkMode)
    {
      ClientHandler* client = serverHandler.findClient(header->destination);
      if(client)
        client->handleMessage(*header, pos + sizeof(DataProtocol::Header), header->size - sizeof(DataProtocol::Header));
      pos += header->size;
      size -= header->size;
      continue;
    }
    handleMessage(*header, pos + sizeof(DataProtocol::Header), header->size - sizeof(DataProtocol::Header));
    pos += header->size;
    size -= header->size;
  }
  if(size >= 5000 && mode != sinkMode)
  {
    client.close();
    return 0;
  }
  return pos - data;
}

void_t ClientHandler::handleMessage(const DataProtocol::Header& messageHeader, byte_t* data, size_t size)
{
  switch((DataProtocol::MessageType)messageHeader.messageType)
  {
  case DataProtocol::registerSourceRequest:
    if(clientAddr == Socket::loopbackAddr && size >= sizeof(DataProtocol::RegisterSourceRequest) && channel == 0)
    {
      DataProtocol::RegisterSourceRequest* request = (DataProtocol::RegisterSourceRequest*)data;
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
          sendErrorResponse((DataProtocol::MessageType)messageHeader.messageType, messageHeader.source, 0, errorMessage);
          return;
        }
      }
      if(channel->getSourceClient())
        channel->getSourceClient()->client.close();
      channel->setSourceClient(this);
      mode = sourceMode;
      byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::RegisterSourceResponse)];
      DataProtocol::Header* header = (DataProtocol::Header*)message;
      DataProtocol::RegisterSourceResponse* registerSourceResponse = (DataProtocol::RegisterSourceResponse*)(header + 1);
      header->size = sizeof(message);
      header->destination = messageHeader.source;
      header->source = 0;
      header->messageType = DataProtocol::registerSourceResponse;
      Memory::copy(registerSourceResponse->channel, (const char_t*)channelName, channelName.length() + 1);
      registerSourceResponse->channelId = channel->getId();
      client.send(message, sizeof(message));
      if(channel->getSinkClient())
      {
        DataProtocol::Header header;
        header.size = sizeof(header);
        header.destination = header.source = 0;
        header.messageType = DataProtocol::registeredSinkMessage;
        client.send((byte_t*)&header, sizeof(header));
      }
    }
    break;
  case DataProtocol::registerSinkRequest:
    if(clientAddr == Socket::loopbackAddr && size >= sizeof(DataProtocol::RegisterSinkRequest) && channel == 0)
    {
      DataProtocol::RegisterSinkRequest* request = (DataProtocol::RegisterSinkRequest*)data;
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
          sendErrorResponse((DataProtocol::MessageType)messageHeader.messageType, messageHeader.source, 0, errorMessage);
          return;
        }
      }
      if(channel->getSinkClient())
        channel->getSinkClient()->client.close();
      channel->setSinkClient(this);
      mode = sinkMode;
      byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::RegisterSinkResponse)];
      DataProtocol::Header* header = (DataProtocol::Header*)message;
      DataProtocol::RegisterSinkResponse* registerSinkResponse = (DataProtocol::RegisterSinkResponse*)(header + 1);
      header->size = sizeof(message);
      header->destination = messageHeader.source;
      header->source = 0;
      header->messageType = DataProtocol::registerSinkResponse;
      Memory::copy(registerSinkResponse->channel, (const char_t*)channelName, channelName.length() + 1);
      registerSinkResponse->channelId = channel->getId();
      client.send(message, sizeof(message));
      if(channel->getSourceClient())
      {
        DataProtocol::Header header;
        header.size = sizeof(header);
        header.destination = header.source = 0;
        header.messageType = DataProtocol::registeredSinkMessage;
        channel->getSourceClient()->client.send((byte_t*)&header, sizeof(header));
      }
    }
    break;
  case DataProtocol::subscribeRequest:
    if(size >= sizeof(DataProtocol::SubscribeRequest))
    {
      DataProtocol::SubscribeRequest* request = (DataProtocol::SubscribeRequest*)data;
      request->channel[sizeof(request->channel) - 1] = '\0';
      String channelName(request->channel, String::length(request->channel));
      Channel* channel = serverHandler.findChannel(channelName);
      if(!channel)
      {
        String errorMessage;
        errorMessage.printf("Unknown channel %s", (const char_t*)channelName);
        sendErrorResponse((DataProtocol::MessageType)messageHeader.messageType, messageHeader.source, 0, errorMessage);
        return;
      }
      uint64_t channelId = channel->getId();
      if(subscriptions.find(channelId) != subscriptions.end())
        return;
      Subscription& subscription = subscriptions.append(channelId, Subscription());
      subscription.channel = channel;
      subscription.lastReplayedTradeId = request->sinceId;

      byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::SubscribeResponse)];
      DataProtocol::Header* header = (DataProtocol::Header*)message;
      DataProtocol::SubscribeResponse* subscribeResponse = (DataProtocol::SubscribeResponse*)(header + 1);
      header->size = sizeof(message);
      header->destination = messageHeader.source;
      header->source = 0;
      header->messageType = DataProtocol::subscribeResponse;
      Memory::copy(subscribeResponse->channel, (const char_t*)channelName, channelName.length() + 1);
      subscribeResponse->channelId = channelId;
      subscribeResponse->flags = 0;

      ClientHandler* sinkClient = channel->getSinkClient();
      if((request->maxAge != 0 || (request->sinceId != 0 && request->sinceId != channel->getLastTradeId())) && sinkClient)
      {
        byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::TradeRequest)];
        DataProtocol::Header* header = (DataProtocol::Header*)message;
        DataProtocol::TradeRequest* tradeRequest = (DataProtocol::TradeRequest*)(header + 1);
        header->size = sizeof(message);
        header->destination = 0;
        header->source = this->id;
        header->messageType = DataProtocol::tradeRequest;
        tradeRequest->sinceId = request->sinceId;
        tradeRequest->maxAge = request->maxAge;
        sinkClient->client.send(message, sizeof(message));
      }
      else
      {
        channel->addListener(*this);
        subscribeResponse->flags |= DataProtocol::syncFlag;
      }

      client.send(message, sizeof(message));
    }
    break;
  case DataProtocol::unsubscribeRequest:
    if(size >= sizeof(DataProtocol::UnsubscribeRequest))
    {
      DataProtocol::UnsubscribeRequest* request = (DataProtocol::UnsubscribeRequest*)data;
      request->channel[sizeof(request->channel) - 1] = '\0';
      String channelName(request->channel, String::length(request->channel));
      Channel* channel = serverHandler.findChannel(channelName);
      if(!channel)
      {
        String errorMessage;
        errorMessage.printf("Unknown channel %s", (const char_t*)channelName);
        sendErrorResponse((DataProtocol::MessageType)messageHeader.messageType, messageHeader.source, 0, errorMessage);
        return;
      }
      uint64_t channelId = channel->getId();
      HashMap<uint64_t, Subscription>::Iterator it = subscriptions.find(channelId);
      if(it == subscriptions.end())
      {
        String errorMessage;
        errorMessage.printf("Not subscribed to %s", (const char_t*)channelName);
        sendErrorResponse((DataProtocol::MessageType)messageHeader.messageType, messageHeader.source, 0, errorMessage);
        return;
      }
      subscriptions.remove(it);
      channel->removeListener(*this);

      byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::UnsubscribeResponse)];
      DataProtocol::Header* header = (DataProtocol::Header*)message;
      DataProtocol::UnsubscribeResponse* unsubscribeResponse = (DataProtocol::UnsubscribeResponse*)(header + 1);
      header->size = sizeof(message);
      header->destination = messageHeader.source;
      header->source = 0;
      header->messageType = DataProtocol::unsubscribeResponse;
      Memory::copy(unsubscribeResponse->channel, (const char_t*)channelName, channelName.length() + 1);
      unsubscribeResponse->channelId = channelId;
      client.send(message, sizeof(message));
    }
    break;
  case DataProtocol::errorResponse:
    if(size >= sizeof(DataProtocol::ErrorResponse))
    {
      DataProtocol::ErrorResponse* errorResponse = (DataProtocol::ErrorResponse*)data;
      switch(errorResponse->messageType)
      {
      case DataProtocol::tradeRequest: // stop requesting trade history and start listening to new trades
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
  case DataProtocol::tradeResponse:
    if(size >= sizeof(DataProtocol::TradeResponse))
    {
      DataProtocol::TradeResponse* tradeResponse = (DataProtocol::TradeResponse*)data;
      size_t count = (size - sizeof(DataProtocol::TradeResponse)) / sizeof(DataProtocol::Trade);
      uint64_t channelId = tradeResponse->channelId;
      HashMap<uint64_t, Subscription>::Iterator it = subscriptions.find(channelId);
      if(it == subscriptions.end())
        return;
      Subscription& subscription = *it;
      byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::TradeMessage)];
      DataProtocol::Header* header = (DataProtocol::Header*)message;
      header->size = sizeof(message);
      header->destination = header->source = 0;
      header->messageType = DataProtocol::tradeMessage;
      uint64_t newestChannelTradeId = subscription.channel->getLastTradeId();
      //client.reserve(sizeof(message) * count);
      for(DataProtocol::Trade* trade = (DataProtocol::Trade*)(tradeResponse + 1), * tradeEnd = trade + count; trade < tradeEnd; ++trade)
      {
        DataProtocol::TradeMessage* treadeMessage = (DataProtocol::TradeMessage*)(header + 1);
        treadeMessage->channelId = channelId;
        treadeMessage->trade.id = trade->id;
        treadeMessage->trade.time = trade->time;
        treadeMessage->trade.price = trade->price;
        treadeMessage->trade.amount = trade->amount;
        treadeMessage->trade.flags = trade->flags | DataProtocol::replayedFlag;
        if(trade->id == newestChannelTradeId)
          treadeMessage->trade.flags |= DataProtocol::syncFlag;
        client.send(message, sizeof(message));
        subscription.lastReplayedTradeId = trade->id;
      }
      if(subscription.lastReplayedTradeId == newestChannelTradeId)
      {
        subscription.channel->addListener(*this);
        const DataProtocol::TickerMessage& tickerMessage = subscription.channel->getLastTicker();
        if(tickerMessage.ticker.time != 0 && tickerMessage.channelId == subscription.channel->getId())
          addedTicker(*subscription.channel, tickerMessage);
      }
      else
        pendingRequests.append(&subscription);
    }
    break;
  case DataProtocol::tradeMessage:
    if(size >= sizeof(DataProtocol::TradeMessage))
    {
      DataProtocol::TradeMessage* tradeMessage = (DataProtocol::TradeMessage*)data;
      if(channel && channel->getId() == tradeMessage->channelId)
        channel->addTrade(tradeMessage->trade);
    }
    break;
  case DataProtocol::tickerMessage:
    if(size >= sizeof(DataProtocol::TickerMessage))
    {
      DataProtocol::TickerMessage* tickerMessage = (DataProtocol::TickerMessage*)data;
      if(channel && channel->getId() == tickerMessage->channelId)
        channel->addTicker(*tickerMessage);
    }
    break;
  case DataProtocol::channelRequest:
    {
      const HashMap<String, Channel*>& channels = serverHandler.getChannels();
      DataProtocol::Header header;
      DataProtocol::Channel channelProt;
      header.size = sizeof(header) + channels.size() * sizeof(DataProtocol::Channel);
      header.destination = messageHeader.source;
      header.source = 0;
      header.messageType = DataProtocol::channelResponse;
      client.send((byte_t*)&header, sizeof(header));
      for(HashMap<String, Channel*>::Iterator i = channels.begin(), end = channels.end(); i != end; ++i)
      {
        const String& channelName = i.key();
        Memory::copy(&channelProt.channel, (const tchar_t*)channelName, channelName.length() + 1);
        client.send((byte_t*)&channelProt, sizeof(channelProt));
      }
    }
    break;
  case DataProtocol::timeRequest:
    {
      byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::TimeResponse)];
      DataProtocol::Header* header = (DataProtocol::Header*)message;
      DataProtocol::TimeResponse* timeRespnse = (DataProtocol::TimeResponse*)(header + 1);
      header->size = sizeof(message);
      header->destination = messageHeader.source;
      header->source = 0;
      header->messageType = DataProtocol::timeResponse;
      timeRespnse->time = Time::time();
      client.send((byte_t*)message, sizeof(message));
    }
    break;
  case DataProtocol::timeMessage:
    if(size >= sizeof(DataProtocol::TimeMessage))
    {
      DataProtocol::TimeMessage* timeMessage = (DataProtocol::TimeMessage*)data;
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
  byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::TradeRequest)];
  DataProtocol::Header* header = (DataProtocol::Header*)message;
  DataProtocol::TradeRequest* tradeRequest = (DataProtocol::TradeRequest*)(header + 1);
  header->size = sizeof(message);
  header->destination = 0;
  header->source = id;
  header->messageType = DataProtocol::tradeRequest;
  tradeRequest->sinceId = subscription->lastReplayedTradeId;
  tradeRequest->maxAge = 0;
  sinkClient->client.send(message, sizeof(message));
}

void_t ClientHandler::addedTrade(Channel& channel, const DataProtocol::Trade& trade)
{
  byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::TradeMessage)];
  DataProtocol::Header* header = (DataProtocol::Header*)message;
  DataProtocol::TradeMessage* tradeMessage = (DataProtocol::TradeMessage*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = DataProtocol::tradeMessage;
  tradeMessage->channelId = channel.getId();
  tradeMessage->trade.id = trade.id;
  tradeMessage->trade.time = channel.toLocalTime(trade.time);
  tradeMessage->trade.price = trade.price;
  tradeMessage->trade.amount = trade.amount;
  tradeMessage->trade.flags = trade.flags;
  client.send(message, sizeof(message));
}

void_t ClientHandler::addedTicker(Channel& channel, const DataProtocol::TickerMessage& tickerMessageSrc)
{
  byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::TickerMessage)];
  DataProtocol::Header* header = (DataProtocol::Header*)message;
  DataProtocol::TickerMessage* tickerMessage = (DataProtocol::TickerMessage*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = DataProtocol::tickerMessage;
  *tickerMessage = tickerMessageSrc;
  tickerMessage->channelId = channel.getId();
  client.send(message, sizeof(message));
}

void_t ClientHandler::sendErrorResponse(DataProtocol::MessageType messageType, uint64_t destination, uint64_t channelId, const String& errorMessage)
{
  byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::ErrorResponse)];
  DataProtocol::Header* header = (DataProtocol::Header*)message;
  DataProtocol::ErrorResponse* errorResponse = (DataProtocol::ErrorResponse*)(header + 1);
  header->size = sizeof(message);
  header->destination = destination;
  header->source = 0;
  header->messageType = DataProtocol::errorResponse;
  errorResponse->messageType = messageType;
  errorResponse->channelId = channelId;
  Memory::copy(errorResponse->errorMessage, (const char_t*)errorMessage, Math::min(errorMessage.length() + 1, sizeof(errorResponse->errorMessage) - 1));
  errorResponse->errorMessage[sizeof(errorResponse->errorMessage) - 1] = '\0';
  client.send(message, sizeof(message));
}
