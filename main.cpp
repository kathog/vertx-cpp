#include <iostream>
#define DCHECK_ALWAYS_ON
#include <hazelcast/client/ClientConfig.h>
#include "src/vertx/ClusteredMessage.h"
#include "src/vertx/vertx.h"
#include "src/vertx/uuid.hpp"
#include "src/vertx/http_server.hpp"
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

    vertx::VertxOptions op;
    op.setConfig(config).setWorkerPoolSize(4);
    op.getEventBusOptions().setEventBusPoolSize(12);

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

    vertx->createNetServer(netOp)->listen(9094, [=] (const evpp::TCPConnPtr& conn, evpp::Buffer* buff)  {

        vertx->eventBus()->request("tarcza2", std::string("uuid::generateUUID()"), [&conn, buff] (const ClusteredMessage& response) {
            const std::string resp = "HTTP/1.1 200 OK\n"
                                     "content-type: application/json\n"
                                     "Date: Sun, 03 May 2020 07:05:15 GMT\n"
                                     "Content-Length: 14\n\n"
                                     "{\"code\": \"UP\"}";
//            LOG_INFO << "request " << response.getBodyAsString();
            conn->Send(resp.c_str(), resp.size());
            buff->Reset();
        });

    });

    http::HttpServer httpServer = vertx->createHttpServer(http::HttpServerOptions{}.setPoolSize(4))->addRoute("/", [&](evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {

        vertx->eventBus()->request("tarcza2", std::string("uuid::generateUUID()"), [ctx, cb] (const ClusteredMessage& response) {
            ctx->AddResponseHeader("content-type", "application/json");
//            ctx->set_response_http_code(200);
            cb("{\"code\": \"UP\"}");
        });

    });

    httpServer.listen(9093);


    vertx->eventBus()->consumer("tarcza", [] (ClusteredMessage& msg) {
//        LOG_INFO << "consumer " <<msg;
        msg.reply(std::string("uuid::generateUUID()"));
    });


    vertx->eventBus()->localConsumer("tarcza2", [](ClusteredMessage &msg) {
        std::string uuid = uuid::generateUUID();
//        LOG_INFO << "consumer " << uuid << " request body: " << msg.getBodyAsString();
        msg.reply(uuid);
    });

    vertx->run();

    return 0;
}
