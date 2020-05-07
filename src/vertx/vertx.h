//
// Created by nerull on 20.04.2020.
//

#ifndef VERTX_TCP_SEASTAR_VERTX_H
#define VERTX_TCP_SEASTAR_VERTX_H

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <evpp/tcp_server.h>

#ifdef _HZ_ENABLE
#include "hazelcast_cluster.h"
#include "event_bus_hazeclast.hpp"
#else
#include "event_bus.hpp"
#endif
#include "net_server.hpp"
#include "http_server.hpp"

using namespace std::chrono_literals;

namespace vertx {

    struct VertxOptions {

        VertxOptions() {
            workerPoolSize = std::thread::hardware_concurrency();
            setVertxHost();
            setVertxPort();
            eventBusOptions.setHost(host).setPort(port);
        }

#ifdef _HZ_ENABLE
        VertxOptions& setConfig(hazelcast::client::ClientConfig config) {
            config.setExecutorPoolSize(1);
            config.getSerializationConfig().addDataSerializableFactory(1001, boost::shared_ptr<serialization::DataSerializableFactory>(new ClusterNodeInfoFactory()));
            this->config = config;
            return *this;
        }
        const ClientConfig &getConfig() const {
            return config;
        }
#endif

        VertxOptions& setWorkerPoolSize(int size) {
            workerPoolSize = size;
            return *this;
        }

        VertxOptions& setHost(std::string&& host) {
            this->host = host;
            return *this;
        }

        VertxOptions& setPort(int port) {
            this->port = port;
            return *this;
        }

        eventbus::EventBusOptions& getEventBusOptions() {
            return this->eventBusOptions;
        }



        int getWorkerPoolSize() const {
            return workerPoolSize;
        }

        const std::string &getHost() const {
            return host;
        }

        int getPort() const {
            return port;
        }

    private:
        eventbus::EventBusOptions eventBusOptions;
#ifdef _HZ_ENABLE
        hazelcast::client::ClientConfig config;
#endif
        int workerPoolSize;
        std::string host;
        int port;

        void setVertxHost() {
            boost::asio::io_service ios;
            boost::system::error_code error;
            host = "46.41.151.244";
            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(host, error), 443);
            boost::asio::ip::tcp::socket socket(ios);
            socket.connect(endpoint);
            host = socket.local_endpoint().address().to_string();
            socket.close();
        }

        void setVertxPort () {
            for (;;) {
                srand (time(nullptr));
                port = rand() % 20000 + 30000;

                boost::asio::io_service ios;
                boost::system::error_code error;
                boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(host, error), port);
                if (!error.failed()) {
                    boost::asio::ip::tcp::socket socket(ios);
                    boost::system::error_code errorSock;
                    socket.bind(endpoint, errorSock);
                    socket.close();
                    if (errorSock.failed()) {
                        break;
                    }
                }
            }
        }
    };

    class Vertx {
    public:

        static std::shared_ptr<Vertx> clusteredVertx (VertxOptions options) {
            std::shared_ptr<Vertx> vertx = std::make_shared<Vertx>();
            vertx->options = options;
#ifdef _HZ_ENABLE
            vertx->_hz = std::make_shared<hazelcast_cluster>(options.getConfig());
            vertx->_hz->join(vertx->options.getPort(), const_cast<std::string &>(vertx->options.getHost()));
            vertx->_eventBus = std::make_shared<eventbus::EventBus>(vertx->_hz, vertx->options.getEventBusOptions());
#else
            vertx->_eventBus = std::make_shared<eventbus::EventBus>(vertx->options.getEventBusOptions());
#endif
            return vertx;
        }

        std::shared_ptr<eventbus::EventBus>& eventBus() {
            return this->_eventBus;
        }

        net::NetServer* createNetServer(net::NetServerOptions options = {}) {
            return new net::NetServer(options);
        }

        http::HttpServer* createHttpServer(http::HttpServerOptions options = {}) {
            return new http::HttpServer(options);
        }

        void run () {
            _workerPool = std::make_shared<evpp::EventLoop>();
            _workerThreadLocal = std::make_shared<evpp::EventLoopThreadPool>(_workerPool.get(), options.getWorkerPoolSize());
            _workerThreadLocal->Start(true);
            std::string addr = options.getHost() + ":" + fmt::format_int(options.getPort()).str();
            LOG_INFO << "worker pool size: " << fmt::format_int(options.getWorkerPoolSize()).str();

            for (uint32_t i = 0; i < options.getWorkerPoolSize(); i++) {
                evpp::EventLoop* next = _workerThreadLocal->GetNextLoop();
                std::shared_ptr<evpp::TCPServer> s(new evpp::TCPServer(next, addr, fmt::format_int(i).str() + "#server", 0));
                s->SetMessageCallback([this] (const evpp::TCPConnPtr& conn, evpp::Buffer* msg) {
                    _eventBus->onMessage(conn, msg);
                });
                s->Init();
                s->Start();
                _tcpServers.push_back(s);
            }
            LOG_INFO << "bind eventbus server to host: [" << options.getHost() << "]:" << fmt::format_int(options.getPort()).str();
            _workerPool->Run();
        }


    private:
#ifdef _HZ_ENABLE
        std::shared_ptr<hazelcast_cluster> _hz;
#endif
        std::shared_ptr<evpp::EventLoop> _workerPool;
        std::shared_ptr<evpp::EventLoopThreadPool> _workerThreadLocal;
        std::vector<std::shared_ptr<evpp::TCPServer>> _tcpServers;
        std::shared_ptr<eventbus::EventBus> _eventBus;
        VertxOptions options;
    };



}


#endif //VERTX_TCP_SEASTAR_VERTX_H
