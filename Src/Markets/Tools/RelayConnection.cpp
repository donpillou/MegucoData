
#include "RelayConnection.h"

bool_t RelayConnection::connect(uint16_t port, const String& channelName)
{
  close();

  // connect to server
  if(!socket.open() ||
      !socket.connect(Socket::loopbackAddr, port) ||
      !socket.setNoDelay())
  {
    error = Socket::getLastErrorString();
    socket.close();
    return false;
  }

  // send register source request
  {
    byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::RegisterSourceRequest)];
    Protocol::Header* header = (Protocol::Header*)message;
    Protocol::RegisterSourceRequest* registerSourceRequest = (Protocol::RegisterSourceRequest*)(header + 1);
    header->size = sizeof(message);
    header->destination = header->source = 0;
    header->messageType = Protocol::registerSourceRequest;
    Memory::copy(registerSourceRequest->channel, channelName, channelName.length() + 1);
    if(!socket.send(message, sizeof(message)))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
  }

  // receive register source response
  {
    Protocol::Header header;
    Protocol::RegisterSinkResponse response;
    if(!socket.recv((byte_t*)&header, sizeof(header)))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
    if(header.messageType != Protocol::registerSourceResponse)
    {
      error = "Could not receive register source response.";
      socket.close();
      return false;
    }
    if(!socket.recv((byte_t*)&response, sizeof(response)))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
    channelId = response.channelId;
  }

  // receive registered sink message
  { // waiting for this message ensures that the sink client is ready to save the data we provide
    Protocol::Header header;
    if(!socket.recv((byte_t*)&header, sizeof(header)))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
    if(header.messageType != Protocol::registeredSinkMessage)
    {
      error = "Could not receive registered sink message.";
      socket.close();
      return false;
    }
  }
  return true;
}

bool_t RelayConnection::sendTrade(const Market::Trade& trade)
{
  byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::TradeMessage)];
  Protocol::Header* header = (Protocol::Header*)message;
  Protocol::TradeMessage* tradeMessage = (Protocol::TradeMessage*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = Protocol::tradeMessage;
  tradeMessage->channelId = channelId;
  tradeMessage->trade.id = trade.id;
  tradeMessage->trade.time = trade.time;
  tradeMessage->trade.price = trade.price;
  tradeMessage->trade.amount = trade.amount;
  tradeMessage->trade.flags = trade.flags;
  if(!socket.send(message, sizeof(message)))
  {
    error = Socket::getLastErrorString();
    socket.close();
    return false;
  }
  return true;
}

bool_t RelayConnection::sendServerTime(uint64_t time)
{
  byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::TimeMessage)];
  Protocol::Header* header = (Protocol::Header*)message;
  Protocol::TimeMessage* timeMessage = (Protocol::TimeMessage*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = Protocol::timeMessage;
  timeMessage->time = time;
  if(!socket.send(message, sizeof(message)))
  {
    error = Socket::getLastErrorString();
    socket.close();
    return false;
  }
  return true;
}

bool_t RelayConnection::sendTicker(const Market::Ticker& ticker)
{
  byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::TickerMessage)];
  Protocol::Header* header = (Protocol::Header*)message;
  Protocol::TickerMessage* tickerMessage = (Protocol::TickerMessage*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = Protocol::tickerMessage;
  tickerMessage->channelId = channelId;
  tickerMessage->time = ticker.time;
  tickerMessage->ask = ticker.ask;
  tickerMessage->bid = ticker.bid;
  if(!socket.send(message, sizeof(message)))
  {
    error = Socket::getLastErrorString();
    socket.close();
    return false;
  }
  return true;
}
