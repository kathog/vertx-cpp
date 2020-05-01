#include <iostream>
//#define DCHECK_ALWAYS_ON
#include <hazelcast/client/ClientConfig.h>
#include "src/vertx/ClusteredMessage.h"
#include "src/vertx/vertx.h"
#include "src/vertx/uuid.hpp"
#include <evpp/dns_resolver.h>
#include <glog/logging.h>

int main(int argc, char* argv[]) {
    FLAGS_logtostderr = 1;
//    FLAGS_logbuflevel = google::GLOG_INFO;
//    FLAGS_colorlogtostderr = 1;
    google::InitGoogleLogging(argv[0]);
    hazelcast::client::ClientConfig config;
    hazelcast::client::Address a{"127.0.0.1", 5701 };
    config.getNetworkConfig().addAddress(a);
    config.setExecutorPoolSize(1);
    config.getSerializationConfig().addDataSerializableFactory(1001, boost::shared_ptr<serialization::DataSerializableFactory>(new ClusterNodeInfoFactory()));

    vertx::VertxOptions op;
    op.setConfig(config).setWorkerPoolSize(4);
    op.getEventBusOptions().setEventBusPoolSize(4);

    std::shared_ptr<vertx::Vertx> vertx = vertx::Vertx::clusteredVertx(op);

    net::NetServerOptions netOp;
    netOp.setPoolSize(8);

    vertx->createNetServer(netOp)->listen(9091, [&] (const evpp::TCPConnPtr& conn, evpp::Buffer* buff)  {

        vertx->eventBus()->request("tarcza", "uuid::generateUUID()", [&conn, buff] (const ClusteredMessage& response) {
            const std::string resp = "HTTP/1.1 200 OK\ncontent-length: 0\n\n";
//            LOG_INFO << "request " << response;
            conn->Send(resp.c_str(), resp.size());
            buff->Reset();
        });

    });

    vertx->createNetServer(netOp)->listen(9092, [=] (const evpp::TCPConnPtr& conn, evpp::Buffer* buff)  {

        vertx->eventBus()->request("dupa", "uuid::generateUUID()", [&conn, buff] (const ClusteredMessage& response) {
            const std::string resp = "HTTP/1.1 200 OK\ncontent-length: 0\n\n";
//            LOG_INFO << "request " << response;
            conn->Send(resp.c_str(), resp.size());
            buff->Reset();
        });

    });

    vertx->createNetServer(netOp)->listen(9093, [=] (const evpp::TCPConnPtr& conn, evpp::Buffer* buff)  {

        vertx->eventBus()->request("tarcza2", "uuid::generateUUID()", [&conn, buff] (const ClusteredMessage& response) {
            const std::string resp = "HTTP/1.1 200 OK\ncontent-length: 0\n\n";
//            LOG_INFO << "request " << response;
            conn->Send(resp.c_str(), resp.size());
            buff->Reset();
        });

    });

    vertx->eventBus()->consumer("tarcza", [] (const ClusteredMessage& msg, ClusteredMessage& response) {
//        LOG_INFO << "consumer " <<msg;
        response.setBody(std::string("uuid::generateUUID()"));
    });


    vertx->eventBus()->localConsumer("tarcza2", [](const ClusteredMessage &msg, ClusteredMessage &response) {
//        LOG_INFO << "consumer " << uuid::generate_uuid_v4();
        response.setBody(uuid::generateUUID());
    });

    vertx->run();

    return 0;
}
