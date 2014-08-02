
#include <nstd/Time.h>
#include <nstd/Directory.h>
#include <nstd/File.h>
#include <nstd/Error.h>
#include <nstd/Console.h>
#include <nstd/List.h>

#include "Tools/Socket.h"
#include "Tools/Math.h"
#include "SinkClient.h"

SinkClient::SinkClient(const String& channelName, uint16_t serverPort) : channelName(channelName), dirName(channelName), serverPort(serverPort), channelId(0), lastTradeId(0)
{
  dirName.replace(' ', '/');
}

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
      byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::RegisterSinkRequest)];
      DataProtocol::Header* header = (DataProtocol::Header*)message;
      DataProtocol::RegisterSinkRequest* registerSinkRequest = (DataProtocol::RegisterSinkRequest*)(header + 1);
      header->size = sizeof(message);
      header->destination = header->source = 0;
      header->messageType = DataProtocol::registerSinkRequest;
      Memory::copy(registerSinkRequest->channel, (const tchar_t*)sinkClient->channelName, sinkClient->channelName.length() + 1);
      if(socket.send(message, sizeof(message)) != sizeof(message))
        break;
    }

    // send subscribe request
    {
      byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::SubscribeRequest)];
      DataProtocol::Header* header = (DataProtocol::Header*)message;
      DataProtocol::SubscribeRequest* subscribeRequest = (DataProtocol::SubscribeRequest*)(header + 1);
      header->size = sizeof(message);
      header->destination = header->source = 0;
      header->messageType = DataProtocol::subscribeRequest;
      Memory::copy(subscribeRequest->channel, (const tchar_t*)sinkClient->channelName, sinkClient->channelName.length() + 1);
      subscribeRequest->maxAge = 0;
      subscribeRequest->sinceId = 0;
      if(socket.send(message, sizeof(message)) != sizeof(message))
        break;
    }

    // receive register sink response
    {
      DataProtocol::Header header;
      if(socket.recv((byte_t*)&header, sizeof(header), sizeof(header)) != sizeof(header))
        break;
      if(header.messageType != DataProtocol::registerSinkResponse)
        break;
      DataProtocol::RegisterSinkResponse response;
      if(socket.recv((byte_t*)&response, sizeof(response), sizeof(response)) != sizeof(response))
        break;
      sinkClient->channelId = response.channelId;
    }

    // receive subscribe response
    {
      DataProtocol::Header header;
      if(socket.recv((byte_t*)&header, sizeof(header), sizeof(header)) != sizeof(header))
        break;
      if(header.messageType != DataProtocol::subscribeResponse)
        break;
      DataProtocol::SubscribeResponse response;
      if(socket.recv((byte_t*)&response, sizeof(response), sizeof(response)) != sizeof(response))
        break;
      if(sinkClient->channelId != response.channelId)
        break;
    }

    // message loop
    DataProtocol::Header header;
    byte_t messageData[100];
    for(;;)
    {
      if(socket.recv((byte_t*)&header, sizeof(header),sizeof(header)) != sizeof(header))
        break;
      size_t messageDataSize = header.size - sizeof(header);
      if(messageDataSize > sizeof(messageData))
        break;
      if(socket.recv(messageData, messageDataSize, messageDataSize) != (ssize_t)messageDataSize)
        break;
      if(!sinkClient->handleMessage(socket, header, messageData, messageDataSize))
        break;
    }
  }

  return 0;
}

void_t SinkClient::loadTradesFromFile()
{
  Directory dir;
  if(!Directory::exists(dirName))
  {
    if(!Directory::create(dirName))
    {
      Console::errorf("error: Could not create directory %s: %s\n", (const tchar_t*)dirName, (const tchar_t*)Error::getErrorString());
      return;
    }
  }
  if(!dir.open(dirName, "trades-*.dat", false))
  {
    Console::errorf("error: Could not open directory %s: %s\n", (const tchar_t*)dirName, (const tchar_t*)Error::getErrorString());
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
    loadTradesFromFile(dirName + "/" + *i);
}

void_t SinkClient::loadTradesFromFile(const String& fileName)
{
  File file;
  if(!file.open(fileName))
  {
    Console::errorf("error: Could not open file %s: %s\n", (const tchar_t*)fileName, (const tchar_t*)Error::getErrorString());
    return;
  }
  byte_t buffer[sizeof(DataProtocol::Trade) * 1000];
  ssize_t bytes;
  while((bytes = file.read(buffer, sizeof(buffer))) > 0)
  {
    for(DataProtocol::Trade* trade = (DataProtocol::Trade*)buffer, * end = trade + bytes / sizeof(DataProtocol::Trade); trade < end; ++trade)
      addTrade(*trade);
    if(bytes % sizeof(DataProtocol::Trade))
    {
      Console::errorf("error: Data from file %s is incomplete: %s\n", (const tchar_t*)fileName, (const tchar_t*)Error::getErrorString());
      return;
    }
  }
  if(bytes == -1)
    Console::errorf("error: Could not read data from %s: %s\n", (const tchar_t*)fileName, (const tchar_t*)Error::getErrorString());
}

bool_t SinkClient::handleMessage(Socket& socket, const DataProtocol::Header& messageHeader, byte_t* data, size_t size)
{
  switch(messageHeader.messageType)
  {
  case DataProtocol::tradeRequest:
    if(size >= sizeof(DataProtocol::TradeRequest))
    {
      DataProtocol::TradeRequest* tradeRequest = (DataProtocol::TradeRequest*)data;
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
          byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::ErrorResponse)];
          DataProtocol::Header* header = (DataProtocol::Header*)message;
          DataProtocol::ErrorResponse* errorResponse = (DataProtocol::ErrorResponse*)(header + 1);
          header->size = sizeof(message);
          header->destination = messageHeader.source;
          header->source = 0;
          header->messageType = DataProtocol::errorResponse;
          errorResponse->messageType = messageHeader.messageType;
          errorResponse->channelId = channelId;
          String errorMessage("Unknown trade id.");
          Memory::copy(errorResponse->errorMessage, (const char_t*)errorMessage, errorMessage.length() + 1);
          if(socket.send(message, sizeof(message)) != sizeof(message))
            break;
        }
        else
        {
          if(itTrade != itTradeEnd)
            ++itTrade;

          byte_t message[4000];
          DataProtocol::Header* header = (DataProtocol::Header*)message;
          header->destination = messageHeader.source;
          header->source = 0;
          header->messageType = DataProtocol::tradeResponse;
          DataProtocol::TradeResponse* tradeResponse = (DataProtocol::TradeResponse*)(header + 1);
          tradeResponse->channelId = channelId;
          DataProtocol::Trade* tradeMsg = (DataProtocol::Trade*)(tradeResponse + 1);
          for(; itTrade != itTradeEnd; ++itTrade)
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
          header->size = (byte_t*)tradeMsg - message;
          if(socket.send(message, header->size) != (ssize_t)header->size)
            return false;
        }
      }
    }
    break;
  case DataProtocol::tradeMessage:
    if(size >= sizeof(DataProtocol::TradeMessage))
      return handleTradeMessage(*(DataProtocol::TradeMessage*)data);
    break;
  default:
    break;
  }
  return true;
}

bool_t SinkClient::handleTradeMessage(DataProtocol::TradeMessage& tradeMessage)
{
  if(tradeMessage.channelId != channelId)
    return true;

  const DataProtocol::Trade& trade = tradeMessage.trade;
  addTrade(tradeMessage.trade);

  // save trade data in file
  String currentfileDate = Time(trade.time, true).toString("%Y-%m-%d");
  if(currentfileDate != fileDate)
  {
    file.close();
    fileDate = currentfileDate;
    fileName = dirName + "/trades-" + fileDate + ".dat";
    if(!file.open(fileName, File::writeFlag | File::appendFlag))
      Console::errorf("error: Could not open file %s: %s\n", (const tchar_t*)fileName, (const tchar_t*)Error::getErrorString());
  }
  if(file.isOpen())
  {
    if(file.write(&tradeMessage.trade, sizeof(DataProtocol::Trade)) != sizeof(DataProtocol::Trade))
      Console::errorf("error: Could not write data to %s: %s\n", (const tchar_t*)fileName, (const tchar_t*)Error::getErrorString());
  }

  return true;
}

void_t SinkClient::addTrade(const DataProtocol::Trade& trade)
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

  lastTradeId = trade.id;
}
