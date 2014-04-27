
#pragma once

#include <nstd/List.h>
#include <nstd/HashSet.h>
#include <nstd/Buffer.h>

#include "Socket.h"

class Server
{
private:
  class ClientSocket;

public:
  class Client
  {
  public:
    class Listener
    {
    public:
      virtual size_t handle(byte_t* data, size_t size) = 0;
      virtual void_t write() = 0;
    };

  public:
    void_t setListener(Listener* listener) {this->listener = listener;}
    Listener* getListener() const {return listener;}
    void_t reserve(size_t capacity) {socket.reserve(capacity);}
    bool_t send(const byte_t* data, size_t size) {return socket.send(data, size);}
    void_t close() {server.close(socket);}

  private:
    Client(Server& server, Server::ClientSocket& socket) : listener(0), server(server), socket(socket) {}

    Listener* listener;
    Server& server;
    Server::ClientSocket& socket;

    friend class Server;
  };

  class Listener
  {
  public:
    virtual void_t acceptedClient(Client& client, uint32_t addr, uint16_t port) = 0;
    virtual void_t closedClient(Client& client) = 0;
  };

  Server() : stopped(false), listener(0) {}

  ~Server()
  {
    for(List<ServerSocket*>::Iterator i = serverSockets.begin(), end = serverSockets.end(); i != end; ++i)
      delete *i;
    for(HashSet<ClientSocket*>::Iterator i = clientSockets.begin(), end = clientSockets.end(); i != end; ++i)
      delete *i;
  }

  void_t setListener(Listener* listener) {this->listener = listener;}

  bool_t listen(uint16_t port)
  {
    ServerSocket* socket = new ServerSocket(*this);
    if(!socket->open() ||
       !socket->setReuseAddress() ||
       !socket->bind(Socket::anyAddr, port) ||
       !socket->listen())
    {
      delete socket;
      return false;
    }
    selector.set(*socket, Socket::Selector::readEvent);
    serverSockets.append(socket);
    return true;
  }
  
  void_t stop()
  {
    stopped = true;
    for(List<ServerSocket*>::Iterator i = serverSockets.begin(), end = serverSockets.end(); i != end; ++i)
      (*i)->close();
  }

  bool_t process()
  {
    Socket* socket;
    uint_t selectEvent;
    while(!stopped)
    {
      if(!selector.select(socket, selectEvent, 1000))
        return false;
      if(socket)
      {
        if(selectEvent & Socket::Selector::writeEvent)
          ((CallbackSocket*)socket)->write();
        if(selectEvent & Socket::Selector::readEvent)
          ((CallbackSocket*)socket)->read();
      }
      if(!socketsToDelete.isEmpty())
      {
        ClientSocket* clientSocket = socketsToDelete.front();
        clientSockets.remove(clientSocket);
        socketsToDelete.remove(socketsToDelete.begin());
        if(listener)
          listener->closedClient(clientSocket->client);
        delete clientSocket;
      }
    }
    return true;
  }

private:
  class CallbackSocket : public Socket
  {
  public:
    virtual void_t read() = 0;
    virtual void_t write() = 0;
  };

  class ClientSocket : public CallbackSocket
  {
  public:
    ClientSocket(Server& server) : server(server), client(server, *this) {}

    void_t reserve(size_t capacity)
    {
      sendBuffer.reserve(sendBuffer.size() + capacity);
    }

    bool_t send(const byte_t* data, size_t size)
    {
      if(sendBuffer.isEmpty())
        server.selector.set(*this, Socket::Selector::readEvent | Socket::Selector::writeEvent);
      sendBuffer.append(data, size);
      return true;
    }

    virtual void_t read()
    {
      size_t bufferSize = recvBuffer.size();
      recvBuffer.resize(bufferSize + 1500);
      ssize_t received = Socket::recv(recvBuffer + bufferSize, 1500);
      switch(received)
      {
      case -1:
        if(getLastError() == 0) // EWOULDBLOCK
          return;
        // no break
      case 0:
        server.close(*this);
        return;
      }
      bufferSize += received;
      recvBuffer.resize(bufferSize);
      size_t handled = client.listener ? client.listener->handle(recvBuffer, bufferSize) : bufferSize;
      recvBuffer.removeFront(handled);
    }

    virtual void_t write()
    {
      if(sendBuffer.isEmpty())
        return;
      ssize_t sent = Socket::send(sendBuffer, sendBuffer.size());
      switch(sent)
      {
      case -1:
        if(getLastError() == 0) // EWOULDBLOCK
          return;
        // no break
      case 0:
        server.close(*this);
        return;
      }
      sendBuffer.removeFront(sent);
      if(sendBuffer.isEmpty())
      {
        sendBuffer.free();
        server.selector.set(*this, Socket::Selector::readEvent);
        if(client.listener)
          client.listener->write();
      }
    }

    Server& server;
    Client client;
    Buffer sendBuffer;
    Buffer recvBuffer;
  };

  class ServerSocket : public CallbackSocket
  {
  public:
    ServerSocket(Server& server) : server(server) {}
    virtual void_t read() {server.accept(*this);}
    virtual void_t write() {}
    Server& server;
  };

  void_t close(ClientSocket& socket)
  {
    if(!socket.isOpen())
      return;
    selector.remove(socket);
    socket.close();
    socketsToDelete.append(&socket);
  }

  //void_t closeAll()
  //{
  //  for(HashSet<ClientSocket*>::Iterator i = clientSockets.begin(), end = clientSockets.end(); i != end; ++i)
  //  {
  //    ClientSocket* clientSocket = *i;
  //    selector.remove(*clientSocket);
  //    clientSocket->close();
  //    if(listener)
  //      listener->closedClient(clientSocket->client);
  //    delete clientSocket;
  //  }
  //  socketsToDelete.clear();
  //  clientSockets.clear();
  //}

  void_t accept(Socket& socket)
  {
    ClientSocket* clientSocket = new ClientSocket(*this);
    uint32_t addr;
    uint16_t port;
    if(!clientSocket->accept(socket, addr, port) ||
       !clientSocket->setNonBlocking() ||
       !clientSocket->setKeepAlive() ||
       !clientSocket->setNoDelay())
    {
      delete clientSocket;
      return;
    }
    selector.set(*clientSocket, Socket::Selector::readEvent);
    clientSockets.append(clientSocket);
    if(listener)
      listener->acceptedClient(clientSocket->client, addr, port);
  }

  volatile bool stopped;
  Listener* listener;
  Socket::Selector selector;
  List<ServerSocket*> serverSockets;
  HashSet<ClientSocket*> clientSockets;
  List<ClientSocket*> socketsToDelete;
};

