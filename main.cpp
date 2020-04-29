#include <iostream>
#include <hazelcast/client/ClientConfig.h>
#include "src/vertx/clustered_message.h"
#include "src/vertx/vertx.h"
#include "src/vertx/uuid.hpp"
#include <evpp/dns_resolver.h>


int main(int argc, char* argv[]) {
    FLAGS_logtostderr = 1;
    FLAGS_logbuflevel = google::GLOG_INFO;
    FLAGS_colorlogtostderr = 1;
    google::InitGoogleLogging(argv[0]);

    hazelcast::client::ClientConfig config;
    hazelcast::client::Address a{"127.0.0.1", 5701 };
    config.getNetworkConfig().addAddress(a);
    config.setExecutorPoolSize(1);

    vertx::VertxOptions op;
    op.setConfig(config).setWorkerPoolSize(2);
    op.getEventBusOptions().setEventBusPoolSize(8);

    std::shared_ptr<vertx::Vertx> vertx = vertx::Vertx::clusteredVertx(op);

    net::NetServerOptions netOp;
    netOp.setPoolSize(4);

    vertx->createNetServer(netOp)->listen(9091, [&] (const evpp::TCPConnPtr& conn, evpp::Buffer* buff)  {

        vertx->eventBus()->request("tarcza", std::string("cyce!"), [&conn, buff] (const clustered_message& response) {
            std::string resp = "HTTP/1.1 200 OK\ncontent-length: 0\n\n";
//            LOG_INFO << "request " << response;
            conn->Send(resp.data(), resp.size());
            buff->Reset();
        });

    });

    vertx->createNetServer(netOp)->listen(9092, [&] (const evpp::TCPConnPtr& conn, evpp::Buffer* buff)  {

        vertx->eventBus()->request("dupa", std::string("cyce!"), [&conn, buff] (const clustered_message& response) {
            std::string resp = "HTTP/1.1 200 OK\ncontent-length: 0\n\n";
//            LOG_INFO << "request " << response;
            conn->Send(resp.data(), resp.size());
            buff->Reset();
        });

    });

    vertx->eventBus()->consumer("tarcza", [] (const clustered_message& msg, clustered_message& response) {
//        LOG_INFO << "consumer " <<msg;
        response.setBody(std::string("dupa tam!"));
    });

    vertx->run();

    return 0;
}
