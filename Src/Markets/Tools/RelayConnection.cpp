
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
    byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::RegisterSourceRequest)];
    DataProtocol::Header* header = (DataProtocol::Header*)message;
    DataProtocol::RegisterSourceRequest* registerSourceRequest = (DataProtocol::RegisterSourceRequest*)(header + 1);
    header->size = sizeof(message);
    header->destination = header->source = 0;
    header->messageType = DataProtocol::registerSourceRequest;
    Memory::copy(registerSourceRequest->channel, channelName, channelName.length() + 1);
    if(socket.send(message, sizeof(message)) != sizeof(message))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
  }

  // receive register source response
  {
    DataProtocol::Header header;
    DataProtocol::RegisterSinkResponse response;
    if(socket.recv((byte_t*)&header, sizeof(header), sizeof(header)) != sizeof(header))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
    if(header.messageType != DataProtocol::registerSourceResponse)
    {
      error = "Could not receive register source response.";
      socket.close();
      return false;
    }
    if(socket.recv((byte_t*)&response, sizeof(response), sizeof(response)) != sizeof(response))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
    channelId = response.channelId;
  }

  // receive registered sink message
  { // waiting for this message ensures that the sink client is ready to save the data we provide
    DataProtocol::Header header;
    if(socket.recv((byte_t*)&header, sizeof(header), sizeof(header)) != sizeof(header))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
    if(header.messageType != DataProtocol::registeredSinkMessage)
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
  byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::TradeMessage)];
  DataProtocol::Header* header = (DataProtocol::Header*)message;
  DataProtocol::TradeMessage* tradeMessage = (DataProtocol::TradeMessage*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = DataProtocol::tradeMessage;
  tradeMessage->channelId = channelId;
  tradeMessage->trade.id = trade.id;
  tradeMessage->trade.time = trade.time;
  tradeMessage->trade.price = trade.price;
  tradeMessage->trade.amount = trade.amount;
  tradeMessage->trade.flags = trade.flags;
  if(socket.send(message, sizeof(message)) != sizeof(message))
  {
    error = Socket::getLastErrorString();
    socket.close();
    return false;
  }
  return true;
}

bool_t RelayConnection::sendServerTime(uint64_t time)
{
  byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::TimeMessage)];
  DataProtocol::Header* header = (DataProtocol::Header*)message;
  DataProtocol::TimeMessage* timeMessage = (DataProtocol::TimeMessage*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = DataProtocol::timeMessage;
  timeMessage->time = time;
  if(socket.send(message, sizeof(message)) != sizeof(message))
  {
    error = Socket::getLastErrorString();
    socket.close();
    return false;
  }
  return true;
}

bool_t RelayConnection::sendTicker(const Market::Ticker& ticker)
{
  byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::TickerMessage)];
  DataProtocol::Header* header = (DataProtocol::Header*)message;
  DataProtocol::TickerMessage* tickerMessage = (DataProtocol::TickerMessage*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = DataProtocol::tickerMessage;
  tickerMessage->channelId = channelId;
  tickerMessage->ticker.time = ticker.time;
  tickerMessage->ticker.ask = ticker.ask;
  tickerMessage->ticker.bid = ticker.bid;
  if(socket.send(message, sizeof(message)) != sizeof(message))
  {
    error = Socket::getLastErrorString();
    socket.close();
    return false;
  }
  return true;
}
