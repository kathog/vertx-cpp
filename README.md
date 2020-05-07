# vertx-cpp

[![Platform](https://img.shields.io/badge/platform-%20%20%20%20Linux-green.svg?style=flat)](https://github.com/kathog/vertx-cpp)
[![License](https://img.shields.io/badge/license-%20%20BSD%203%20clause-yellow.svg?style=flat)](LICENSE)
[![Project Status: WIP – Initial development is in progress, but there has not yet been a stable, usable release suitable for the public.](https://www.repostatus.org/badges/latest/wip.svg)](https://www.repostatus.org/#wip)

# Introduction

[vertx-cpp](https://github.com/kathog/vertx-cpp) its a simple implementation of [vert.x](https://github.com/eclipse-vertx/vert.x) in C++. The vertx-cpp engine is based on [evpp](https://github.com/Qihoo360/evpp) as a multi-threaded nonblocking event-driven engine.
Currently, the only implementation is the cluster version based on hazelcast as a cluster manager. In the future there will also be a standalone version and other implementations of the cluster manager

# Features

1. Nonblocking eventbus consumer / localConsumer
2. Nonblocking eventbus request
3. Nonblocking multi-threaded tcp server
3. Nonblocking multi-threaded http server

# Getting Started
## Dependencies
To develop with [vertx-cpp](https://github.com/kathog/vertx-cpp) we need to gets 3 dependencies:
1. Conan from https://conan.io
2. Hazelcast client from https://github.com/hazelcast/hazelcast-cpp-client
3. Evpp from https://github.com/Qihoo360/evpp

In addition to dependencies, at least one Java Vert.x instance is required to work in cluster. In addition, the following code changes are needed:
1. Change class: HazelcastClusterNodeInfo to [HazelcastClusterNodeInfo](https://github.com/kathog/vertx-cpp/blob/master/java/HazelcastClusterNodeInfo.java)
2. Add this code to your Vert.x application config:
```java
...
config.getSerializationConfig().addDataSerializableFactoryClass(1001, HazelcastClusterNodeInfo.class);
...
mgr.getHazelcastInstance().getClientService().addClientListener(new ClientListener() {
    @Override
    public void clientConnected(Client client) {
        Address a = new Address(client.getSocketAddress());
        Member m = new MemberImpl(a, MemberVersion.UNKNOWN, false, client.getUuid());
        MembershipEvent event = new MemberAttributeEvent(mgr.getHazelcastInstance().getCluster(), m, MemberAttributeOperationType.PUT, client.getUuid(), null);
        mgr.memberAdded(event);
    }

    @Override
    public void clientDisconnected(Client client) {
        System.out.println(client);
        Address a = new Address(client.getSocketAddress());
        Member m = new MemberImpl(a, MemberVersion.UNKNOWN, false, client.getUuid());
        MembershipEvent event0 = new MemberAttributeEvent(mgr.getHazelcastInstance().getCluster(), m, MemberAttributeOperationType.REMOVE, client.getUuid(), null);
        mgr.memberRemoved(event0);

    }
});
```



## Code examples

### Eventbus consumer
consumer:

```cpp
#include <iostream>
#include <hazelcast/client/ClientConfig.h>
#include "vertx/ClusteredMessage.h"
#include "vertx/vertx.h"

int main(int argc, char* argv[]) {
  hazelcast::client::ClientConfig config;
  vertx::VertxOptions op;
  op.setConfig(config);

  std::shared_ptr<vertx::Vertx> vertx = vertx::Vertx::clusteredVertx(op);
  vertx->eventBus()->consumer("consumer", [] (ClusteredMessage& msg) {
      msg.reply(std::string("pong"));
  });
  vertx->run();

  return 0;
}
```

local consumer:

```cpp
#include <iostream>
#include <hazelcast/client/ClientConfig.h>
#include "vertx/ClusteredMessage.h"
#include "vertx/vertx.h"

int main(int argc, char* argv[]) {
  hazelcast::client::ClientConfig config;
  vertx::VertxOptions op;
  op.setConfig(config);

  std::shared_ptr<vertx::Vertx> vertx = vertx::Vertx::clusteredVertx(op);
  vertx->eventBus()->localConsumer("consumer", [] (ClusteredMessage& msg) {
      msg.reply(std::string("pong"));
  });
  vertx->run();

  return 0;
}
```

### Eventbus request
request with handle response

```cpp
#include <iostream>
#include <hazelcast/client/ClientConfig.h>
#include "vertx/ClusteredMessage.h"
#include "vertx/vertx.h"

int main(int argc, char* argv[]) {
  hazelcast::client::ClientConfig config;
  vertx::VertxOptions op;
  op.setConfig(config);

  std::shared_ptr<vertx::Vertx> vertx = vertx::Vertx::clusteredVertx(op);
  vertx->eventBus()->request("consumer", std::string("ping"), [] (ClusteredMessage& response) {
      std::cout << response.bodyAsString() << std::endl;
  });
  vertx->run();

  return 0;
}
```

### Tcp server

```cpp
#include <iostream>
#include <hazelcast/client/ClientConfig.h>
#include "vertx/ClusteredMessage.h"
#include "vertx/vertx.h"

int main(int argc, char* argv[]) {
  hazelcast::client::ClientConfig config;
  vertx::VertxOptions op;
  op.setConfig(config);

  std::shared_ptr<vertx::Vertx> vertx = vertx::Vertx::clusteredVertx(op);
  net::NetServerOptions netOp;

  vertx->createNetServer(netOp)->listen(9091, [&] (const evpp::TCPConnPtr& conn, evpp::Buffer* buff)  {
      conn->Send(std::string("hello word"));
      buff->Reset();
  });
  vertx->run();

  return 0;
}
```

### Http server
```cpp
#include <iostream>
#include <hazelcast/client/ClientConfig.h>
#include "vertx/ClusteredMessage.h"
#include "vertx/vertx.h"
#include "vertx/http_server.hpp"

int main(int argc, char* argv[]) {
  hazelcast::client::ClientConfig config;
  vertx::VertxOptions op;
  op.setConfig(config);

  std::shared_ptr<vertx::Vertx> vertx = vertx::Vertx::clusteredVertx(op);

  http::HttpServer httpServer = vertx->createHttpServer(http::HttpServerOptions{}.setPoolSize(1))->addRoute("/", [&](evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
    ctx->AddResponseHeader("content-type", "application/json");
    cb("{\"hello\": \"world\"}");
  });

  vertx->run();

  return 0;
}
```

## Build
Clone repository
```
git clone https://github.com/kathog/vertx-cpp.git
```
Create build directory
```
cd vertx-cpp
mkdir build
cd build
```
### Install conan dependencies
```
conan install ../ -if=. -pr=default
```
### Cmake
You must have the same base directory as the hazelcast directory or change the hazelcast directory in CMakeLists.txt.

```
cmake .. -DCMAKE_BUILD_TYPE=Release -D_HZ_ENABLE=ON
```
or if you want to use the stadalon version
```
cmake .. -DCMAKE_BUILD_TYPE=Release -D_HZ_ENABLE=OFF
```
compile:
```
make
```
