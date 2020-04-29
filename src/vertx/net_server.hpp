//
// Created by nerull on 27.04.2020.
//

#ifndef VERTX_CPP_ASIO_NET_SERVER_HPP
#define VERTX_CPP_ASIO_NET_SERVER_HPP

#include <evpp/tcp_callbacks.h>
#include <evpp/event_loop.h>
#include <evpp/event_loop_thread_pool.h>
#include <evpp/tcp_server.h>
#include <future>

using namespace std::chrono_literals;

namespace net {

    struct NetServerOptions {
    public:

        NetServerOptions& setPoolSize(int size) {
            poolSize = size;
            return *this;
        }

        int getPoolSize() const {
            return poolSize;
        }

    private:
        int poolSize = 1;

    };

class NetServer {

    private:
        evpp::MessageCallback _cb;
        NetServerOptions _options;
        std::shared_ptr<evpp::EventLoop> _workerPool;
        std::shared_ptr<evpp::EventLoopThreadPool> _workerThreadLocal;
        std::vector<std::shared_ptr<evpp::TCPServer>> _tcpServers;
        std::thread th;

    public:

        NetServer(const NetServerOptions &options) : _options(options) {}

        void listen (int port, const evpp::MessageCallback cb) {
            this->_cb = cb;
            start(port);
        }

        void start (int port) {
            th = std::thread([this, port] () {
                _workerPool = std::make_shared<evpp::EventLoop>();
                _workerThreadLocal = std::make_shared<evpp::EventLoopThreadPool>(_workerPool.get(), _options.getPoolSize());
                _workerThreadLocal->Start(true);
                std::string addr = "0.0.0.0:" + std::to_string(port);
                for (uint32_t i = 0; i < _options.getPoolSize(); i++) {
                    evpp::EventLoop* next = _workerThreadLocal->GetNextLoop();
                    std::shared_ptr<evpp::TCPServer> s(new evpp::TCPServer(next, addr, std::to_string(i) + "#server", 0));
                    s->SetMessageCallback([this] (const evpp::TCPConnPtr& conn, evpp::Buffer* msg) {
                        this->_cb(conn, msg);
                    });
                    s->Init();
                    s->Start();
                    _tcpServers.push_back(s);
                }
                LOG_INFO << "bind net server to host: [0.0.0.0]:" << std::to_string(port);
                _workerPool->Run();
            });
            th.detach();
        }


    };

}

#endif //VERTX_CPP_ASIO_NET_SERVER_HPP
