
#include <zlimdbclient.h>

#include "ZlimdbConnection.h"

struct zlimdb_trade_entity
{
  zlimdb_entity entity;
  double price;
  double amount;
  uint32_t flags;
};

struct zlimdb_ticker_entity
{
public:
  zlimdb_entity entity;
  double bid;
  double ask;
};

bool_t ZlimdbConnection::connect(const String& channelName)
{
  close();

  // connect to server
  zdb = zlimdb_create(0, 0);
  if(!zdb)
  {
    error.printf("%s.", zlimdb_strerror(zlimdb_errno()));
    return false;
  }
  if(zlimdb_connect(zdb, 0, 0, "root", "root") != 0)
  {
    error.printf("%s.", zlimdb_strerror(zlimdb_errno()));
    zlimdb_free(zdb);
    zdb = 0;
    return false;
  }

  // create trades table
  if(zlimdb_add_table(zdb, channelName + "/trades", &tradesTableId) != 0)
  {
    error.printf("%s.", zlimdb_strerror(zlimdb_errno()));
    zlimdb_free(zdb);
    zdb = 0;
    return false;
  }

  // create ticker table
  if(zlimdb_add_table(zdb, channelName + "/ticker", &tradesTableId) != 0)
  {
    error.printf("%s.", zlimdb_strerror(zlimdb_errno()));
    zlimdb_free(zdb);
    zdb = 0;
    return false;
  }
  return true;
}

void_t ZlimdbConnection::close()
{
  if(zdb)
  {
    zlimdb_free(zdb);
    zdb = 0;
  }
}

bool_t ZlimdbConnection::isOpen() const
{
  return zlimdb_is_connected(zdb) == 0;
}

bool_t ZlimdbConnection::sendTrade(const Market::Trade& trade)
{
  zlimdb_trade_entity tradeEntity;
  tradeEntity.entity.id = trade.id;
  tradeEntity.entity.time = trade.time;
  tradeEntity.entity.size = sizeof(tradeEntity);
  tradeEntity.amount = trade.amount;
  tradeEntity.price = trade.price;
  tradeEntity.flags = trade.flags;
  if(zlimdb_add(zdb, tradesTableId, &tradeEntity.entity) != 0)
  {
    error.printf("%s.", zlimdb_strerror(zlimdb_errno()));
    return false;
  }
  return true;
}

bool_t ZlimdbConnection::sendTicker(const Market::Ticker& ticker)
{
  zlimdb_ticker_entity tickerEntity;
  tickerEntity.entity.id = 0;
  tickerEntity.entity.time = ticker.time;
  tickerEntity.entity.size = sizeof(tickerEntity);
  tickerEntity.ask = ticker.ask;
  tickerEntity.bid = ticker.bid;
  if(zlimdb_add(zdb, tradesTableId, &tickerEntity.entity) != 0)
  {
    error.printf("%s.", zlimdb_strerror(zlimdb_errno()));
    return false;
  }
  return true;
}
