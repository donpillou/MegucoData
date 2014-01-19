
#ifndef _WIN32
#include <unistd.h>
#endif

#include <nstd/Console.h>
#include <nstd/Debug.h>
#include <nstd/Directory.h>
#include <nstd/Error.h>

#include "Tools/Server.h"
#include "ServerHandler.h"

int_t main(int_t argc, char_t* argv[])
{
  static const uint16_t port = 40123;
  bool background = true;
  const char* dataDir = 0;

  // parse parameters
  for(int i = 1; i < argc; ++i)
    if(String::compare(argv[i], "-f") == 0)
      background = false;
    else if(String::compare(argv[i], "-c") == 0&& i + 1 < argc)
      dataDir = argv[++i];
    else
    {
      Console::errorf("Usage: %s [-b] [-c <dir>]\n\
  -f            run in foreground (not as daemon)\n\
  -c <dir>      set database directory (default is .)\n", argv[0]);
      return -1;
    }

  // change working directory
  if(dataDir && *dataDir && !Directory::change(String(dataDir, String::length(dataDir))))
  {
    Console::errorf("error: Could not enter database directory: %s\n", (const tchar_t*)Error::getErrorString());
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

  Console::printf("Starting relay server...\n", port);

  // initialize listen server
  Server server;
  ServerHandler serverHandler(port);
  server.setListener(&serverHandler);

  // run listen server
  if(!server.listen(port))
  {
    Console::errorf("error: Could not listen on port %hu: %s\n", port, (const char_t*)Socket::getLastErrorString());
    return -1;
  }

  Console::printf("Listening on port %hu.\n", port);

  if(!server.process())
  {
    Console::errorf("error: Could not run select loop: %s\n", (const char_t*)Socket::getLastErrorString());
    return -1;
  }
  return 0;
}

