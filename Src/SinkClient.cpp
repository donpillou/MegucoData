
#include <nstd/Time.h>
#include <nstd/Directory.h>
#include <nstd/File.h>
#include <nstd/Error.h>
#include <nstd/Console.h>
#include <nstd/List.h>

#include "Tools/Socket.h"
#include "Tools/Math.h"
#include "SinkClient.h"

uint_t SinkClient::main(void_t* param)
{
  SinkClient* sinkClient = (SinkClient*)param;

  // load trade data from files
  sinkClient->loadTradesFromFile();

  //
  for(;;) for(;;)
  {
    // connect to server
    Socket socket;
    if(!socket.open() ||
       !socket.connect(Socket::loopbackAddr, sinkClient->serverPort))
      break;

    // send register sink request
    {
      byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::RegisterSinkRequest)];
      Protocol::Header* header = (Protocol::Header*)message;
      Protocol::RegisterSinkRequest* registerSinkRequest = (Protocol::RegisterSinkRequest*)(header + 1);
      header->size = sizeof(message);
      header->destination = header->source = 0;
      header->messageType = Protocol::registerSinkRequest;
      Memory::copy(registerSinkRequest->channel, (const tchar_t*)sinkClient->channelName, sinkClient->channelName.length() + 1);
      if(!socket.send(message, sizeof(message)))
        break;
    }

    // send subscribe request
    {
      byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::SubscribeRequest)];
      Protocol::Header* header = (Protocol::Header*)message;
      Protocol::SubscribeRequest* subscribeRequest = (Protocol::SubscribeRequest*)(header + 1);
      header->size = sizeof(message);
      header->destination = header->source = 0;
      header->messageType = Protocol::subscribeRequest;
      Memory::copy(subscribeRequest->channel, (const tchar_t*)sinkClient->channelName, sinkClient->channelName.length() + 1);
      subscribeRequest->maxAge = 0;
      if(!socket.send(message, sizeof(message)))
        break;
    }

    // receive register sink response
    {
      Protocol::Header header;
      if(!socket.recv((byte_t*)&header, sizeof(header)))
        break;
      if(header.messageType != Protocol::registerSinkResponse)
        break;
      Protocol::RegisterSinkResponse response;
      if(!socket.recv((byte_t*)&response, sizeof(response)))
        break;
      sinkClient->channelId = response.channelId;
    }

    // receive subscribe response
    {
      Protocol::Header header;
      if(!socket.recv((byte_t*)&header, sizeof(header)))
        break;
      if(header.messageType != Protocol::subscribeResponse)
        break;
      Protocol::SubscribeResponse response;
      if(!socket.recv((byte_t*)&response, sizeof(response)))
        break;
      if(sinkClient->channelId != response.channelId)
        break;
    }

    // message loop
    Protocol::Header header;
    byte_t messageData[100];
    for(;;)
    {
      if(!socket.recv((byte_t*)&header, sizeof(header)))
        break;
      size_t messageDataSize = header.size - sizeof(header);
      if(messageDataSize > sizeof(messageData))
        break;
      if(!socket.recv(messageData, messageDataSize))
        break;
      if(!sinkClient->handleMessage(socket, (Protocol::MessageType)header.messageType, messageData, messageDataSize))
        break;
    }
  }

  return 0;
}

void_t SinkClient::loadTradesFromFile()
{
  Directory dir;
  Directory::create(channelName);
  //if(!dir.open(channelName, "trades-*.dat", false))
  //if(!dir.open(channelName, "trades-*.*", false))
  if(!dir.open(channelName, "*", false))
  {
    Console::errorf("error: Could not open directory %s: %s\n", (const tchar_t*)channelName, (const tchar_t*)Error::getErrorString());
    return;
  }
  String path;
  bool_t isDir;
  List<String> files;
  while(dir.read(path, isDir))
  {
    if(isDir)
      continue;
    files.append(path);
  }
  files.sort();
  while(files.size() > 7)
    files.removeFront();
  for(List<String>::Iterator i = files.begin(), end = files.end(); i != end; ++i)
    loadTradesFromFile(channelName + "/" + *i);
}

void_t SinkClient::loadTradesFromFile(const String& fileName)
{
  File file;
  if(!file.open(fileName))
  {
    Console::errorf("error: Could not open file %s: %s\n", (const tchar_t*)fileName, (const tchar_t*)Error::getErrorString());
    return;
  }
  byte_t buffer[sizeof(Protocol::Trade) * 1000];
  ssize_t bytes;
  while((bytes = file.read(buffer, sizeof(buffer))) > 0)
  {
    for(Protocol::Trade* trade = (Protocol::Trade*)buffer, * end = trade + bytes / sizeof(buffer); trade < end; ++trade)
      addTrade(*trade);
    if(bytes % sizeof(buffer))
    {
      Console::errorf("error: Data from file %s is incomplete: %s\n", (const tchar_t*)fileName, (const tchar_t*)Error::getErrorString());
      return;
    }
  }
  if(bytes == -1)
    Console::errorf("error: Could not read data from %s: %s\n", (const tchar_t*)fileName, (const tchar_t*)Error::getErrorString());
}

bool_t SinkClient::handleMessage(Socket& socket, Protocol::MessageType messageType, byte_t* data, size_t size)
{
  switch(messageType)
  {
  case Protocol::tradeRequest:
    if(size >= sizeof(Protocol::TradeRequest))
    {
      Protocol::TradeRequest* tradeRequest = (Protocol::TradeRequest*)data;
      if(!trades.isEmpty() && !keyTrades.isEmpty())
      {
        if(tradeRequest->maxAge != 0 && tradeRequest->sinceId == 0)
        {
          uint64_t time = trades.back().time;
          uint64_t minTime = time - tradeRequest->maxAge;

          HashMap<uint64_t, uint64_t>::Iterator itKey = keyTrades.find(minTime / (60ULL * 60ULL * 1000ULL));
          if(itKey == keyTrades.end())
            itKey = keyTrades.begin();
          HashMap<uint64_t, Trade>::Iterator itTrade = trades.find(*itKey);
          if(itTrade == trades.end())
            itTrade = trades.begin();
          HashMap<uint64_t, Trade>::Iterator itTradeEnd = trades.end();
          while(itTrade != itTradeEnd && (*itTrade).time < minTime)
            ++itTrade;
          if(itTrade != trades.begin())
            --itTrade;
          tradeRequest->sinceId = itTrade.key();
        }
        else if(tradeRequest->sinceId == 0)
        {
          HashMap<uint64_t, Trade>::Iterator itTrade = --HashMap<uint64_t, Trade>::Iterator(trades.end());
          HashMap<uint64_t, Trade>::Iterator itTradeBegin = trades.begin();
          for(int i = 0; i < 101; ++i, --itTrade)
            if(itTrade == itTradeBegin)
              break;
          tradeRequest->sinceId = itTrade.key();
        }
        HashMap<uint64_t, Trade>::Iterator itTrade = trades.find(tradeRequest->sinceId);
        HashMap<uint64_t, Trade>::Iterator itTradeEnd = trades.end();
        if(itTrade == itTradeEnd)
        {
          byte_t message[sizeof(Protocol::Header) + sizeof(Protocol::TradeResponse)];
          Protocol::Header* header = (Protocol::Header*)message;
          Protocol::TradeResponse* tradeResponse = (Protocol::TradeResponse*)(header + 1);
          header->destination = header->source = 0;
          header->messageType = Protocol::tradeErrorResponse;
          tradeResponse->channelId = channelId;
          if(!socket.send(message, sizeof(message)))
            break;
        }
        ++itTrade;

        byte_t message[4000];
        Protocol::Header* header = (Protocol::Header*)message;
        header->destination = header->source = 0;
        header->messageType = Protocol::tradeResponse;
        Protocol::TradeResponse* tradeResponse = (Protocol::TradeResponse*)(header + 1);
        tradeResponse->channelId = channelId;
        Protocol::Trade* tradeMsg = (Protocol::Trade*)(tradeResponse + 1);
        while(itTrade != itTradeEnd)
        {
          Trade& trade = *itTrade;
          tradeMsg->id = itTrade.key();
          tradeMsg->time = trade.time;
          tradeMsg->price = trade.price;
          tradeMsg->amount = trade.amount;
          tradeMsg->flags = trade.flags;
          ++tradeMsg;
          if((size_t)((byte_t*)(tradeMsg + 1) - message) > sizeof(message))
            break;
        }
        header->size = (byte_t*)(tradeMsg + 1) - message;
        if(!socket.send(message, header->size))
          return false;
      }

    }
    break;
  case Protocol::tradeMessage:
    if(size >= sizeof(Protocol::TradeMessage))
      return handleTradeMessage(*(Protocol::TradeMessage*)data);
    break;
  default:
    break;
  }
  return true;
}

bool_t SinkClient::handleTradeMessage(Protocol::TradeMessage& tradeMessage)
{
  if(tradeMessage.channelId != channelId)
    return true;
  if(tradeMessage.trade.id <= lastTradeId)
    return true;

  uint64_t time = tradeMessage.trade.time;
  Trade& trade = trades.append(tradeMessage.trade.id, Trade());
  trade.time = time;
  trade.price = tradeMessage.trade.price;
  trade.amount = tradeMessage.trade.amount;
  trade.flags = tradeMessage.trade.flags;

  uint64_t keyTime = time / (60ULL * 60ULL * 1000ULL);
  if(keyTrades.find(keyTime) == keyTrades.end())
    keyTrades.append(keyTime, tradeMessage.trade.id);

  while(time - trades.front().time > 7ULL * 24ULL * 60ULL * 60ULL * 1000ULL)
    trades.removeFront();
  while(keyTime - keyTrades.begin().key() > 7ULL * 24ULL)
    keyTrades.removeFront();

  // save trade data in file
  String currentfileDate = Time::toString(trade.time, "%Y-%m-%d");
  if(currentfileDate != fileDate)
  {
    file.close();
    fileDate = currentfileDate;
    fileName = channelName + "/trades-" + fileDate + ".dat";
    if(!file.open(fileName))
      Console::errorf("error: Could not open file %s: %s\n", (const tchar_t*)fileName, (const tchar_t*)Error::getErrorString());
  }
  if(file.isOpen())
  {
    if(file.write(&tradeMessage.trade, sizeof(Protocol::Trade)) != sizeof(Protocol::Trade))
      Console::errorf("error: Could not write data to %s: %s\n", (const tchar_t*)fileName, (const tchar_t*)Error::getErrorString());
  }

  return true;
}

void_t SinkClient::addTrade(const Protocol::Trade& trade)
{
 if(trade.id <= lastTradeId)
    return;

  uint64_t time = trade.time;
  Trade& newTrade = trades.append(trade.id, Trade());
  newTrade.time = time;
  newTrade.price = trade.price;
  newTrade.amount = trade.amount;
  newTrade.flags = trade.flags;

  uint64_t keyTime = time / (60ULL * 60ULL * 1000ULL);
  if(keyTrades.find(keyTime) == keyTrades.end())
    keyTrades.append(keyTime, trade.id);

  while(time - trades.front().time > 7ULL * 24ULL * 60ULL * 60ULL * 1000ULL)
    trades.removeFront();
  while(keyTime - keyTrades.begin().key() > 7ULL * 24ULL)
    keyTrades.removeFront();
}
