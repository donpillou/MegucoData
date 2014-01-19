
#ifndef _WIN32
#include <unistd.h>
#endif

#include <nstd/Console.h>
#include <nstd/Thread.h>

#include "Tools/RelayConnection.h"

#include "Markets/BitstampUsd.h"
typedef BitstampUsd MarketConnection;

int_t main(int_t argc, char_t* argv[])
{
  static const uint16_t port = 40123;
  bool background = true;

  // parse parameters
  for(int i = 1; i < argc; ++i)
    if(String::compare(argv[i], "-f") == 0)
      background = false;
    else
    {
      Console::errorf("Usage: %s [-b]\n\
  -f            run in foreground (not as daemon)\n", argv[0]);
      return -1;
    }

#ifndef _WIN32
  // daemonize process
  if(background)
  {
    Console::printf("Starting as daemon...\n");
    pid_t childPid = fork();
    if(childPid == -1)
      return -1;
    if(childPid != 0)
      return 0;
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }
#endif

  RelayConnection relayConnection;
  MarketConnection marketConnection;
  String channelName = marketConnection.getChannelName();

  class Callback : public Market::Callback
  {
  public:
    virtual bool_t receivedTrade(const Market::Trade& trade)
    {
      if(!relayConnection->send(trade))
      {
        // save trade in buffer, send it to relay when the connection is back
        return false;
      }
      return true;
    }

    RelayConnection* relayConnection;
  } callback;
  callback.relayConnection = &relayConnection;

  for(;; Thread::sleep(10 * 1000))
  {
    if(!relayConnection.isOpen())
    {
      Console::printf("Connecting to relay server...\n");
      if(!relayConnection.connect(port, channelName))
      {
        Console::printf("Could not connect to relay server: %s\n", (const char_t*)relayConnection.getErrorString());
        continue;
      }
      else
        Console::printf("Connected to relay server.\n");
    }

    if(!marketConnection.isOpen())
    {
      Console::printf("Connecting to %s...\n", (const char_t*)channelName);
      if(!marketConnection.connect())
      {
        Console::printf("Could not connect to %s: %s\n", (const char_t*)channelName, (const char_t*)relayConnection.getErrorString());
        continue;
      }
      else
        Console::printf("Connected to %s.\n", (const char_t*)channelName);
    }

    for(;;)
      if(!marketConnection.process(callback))
        break;

    if(!relayConnection.isOpen())
      Console::printf("Lost connection to relay server: %s\n", (const char_t*)relayConnection.getErrorString());
    if(!marketConnection.isOpen())
      Console::printf("Lost connection to %s: %s\n", (const char_t*)channelName, (const char_t*)relayConnection.getErrorString());
  }

  return 0;
}
