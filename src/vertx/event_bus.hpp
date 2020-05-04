//
// Created by nerull on 23.04.2020.
//

#ifndef VERTX_CPP_ASIO_EVENT_BUS_HPP
#define VERTX_CPP_ASIO_EVENT_BUS_HPP

#include <string>
#include <mutex>
#include <thread>
#include <evpp/event_loop.h>
#include <evpp/event_loop_thread_pool.h>
#include "ClusteredMessage.h"
#include "uuid.hpp"
#include <evpp/buffer.h>
#include <evpp/tcp_conn.h>
#include <future>
#include <evpp/tcp_client.h>
#include <moodycamel/concurrentqueue.h>
#include <moodycamel/blockingconcurrentqueue.h>
#include <fmt/format.h>
#include <boost/thread/mutex.hpp>


using namespace std::chrono_literals;

namespace eventbus {

    struct EventBusOptions {

    public:
        EventBusOptions() {
            this->eventBusPoolSize = std::thread::hardware_concurrency();
        }

        int getEventBusPoolSize() const {
            return eventBusPoolSize;
        }

        const std::string &getHost() const {
            return host;
        }

        int getPort() const {
            return port;
        }

        EventBusOptions& setEventBusPoolSize(int size) {
            this->eventBusPoolSize = size;
            return *this;
        }

        EventBusOptions& setHost(std::string host) {
            this->host = host;
            return *this;
        }

        EventBusOptions& setPort(int port) {
            this->port = port;
            return *this;
        }

    private:
        int eventBusPoolSize;
        std::string host;
        int port;


    };

    class EventBus {
    private:
        EventBusOptions options;
        std::shared_ptr<hazelcast_cluster> hz;
        std::shared_ptr<evpp::EventLoop> _eventBusPool;
        std::shared_ptr<evpp::EventLoopThreadPool> _eventBusThreadLocal;
        std::unordered_map<std::string, MsgCallback> _consumers;
        std::unordered_map<std::string, MsgCallback> _consumersLocal;
        std::unordered_map<std::string, MsgCallback> _publishers;
        std::vector<std::future<void>> evConnect;
        std::unordered_map<long, std::shared_ptr<evpp::TCPClient>> endpoints;
        std::vector<std::future<void>> fs;
        std::mutex _resp_mutex;

    public:
        EventBus(std::shared_ptr<hazelcast_cluster> hz, EventBusOptions& options) : hz(hz), options(options) {
            allocateEventLoop();
        }

        /**
         * Add consumer to eventbus
         *
         * @param address - address of conumser in eventbus
         * @param function - invoke function on consumer
         */
        void consumer (std::string&& address, MsgCallback function) {
            _consumers.emplace(address, function);
            hz->addSub(address, options.getPort(), const_cast<std::string &>(options.getHost()));
        }

        /**
         * Add a local consumer to eventbus
         *
         * @param address - address of conumser in eventbus
         * @param function - invoke function on consumer
         */
        void localConsumer (std::string&& address, MsgCallback function) {
            _consumersLocal.emplace(address, function);
        }

        /**
         * Request query to eventbus
         *
         * @param address - consumer address
         * @param value - value to send
         * @param func - response handler function
         */
        void request (std::string&& address, std::any value, RequestMsgCallback func) {
            _eventBusThreadLocal->GetNextLoop()->QueueInLoop([this, address = std::move(address), value = std::move(value), func = std::move(func)]() {
                auto it = _consumersLocal.find(address);
                if (it == _consumersLocal.cend()) {
                    std::string uuid_ = "__vertx.reply." + uuid::generateUUID();
                    ServerID server_ = hz->next(address);
                    ClusteredMessage request_message = ClusteredMessage{0, 1, 9, true, uuid_, address, server_.getPort(), server_.getHost(), 4, value};
                    request_message.setRequest(true);
                    {
                    std::unique_lock<std::mutex> lock(_resp_mutex);
                        _publishers.emplace(uuid_, func);
                    }
                    processOnTcpMessage(std::move(request_message));
                } else {
                    ClusteredMessage request_message = ClusteredMessage{0, 1, 9, true, "__vertx.reply.local", address, options.getPort(), options.getHost(), 4, value};
                    request_message.setRequest(true);
                    request_message.setFunc(it->second, func);
                    processOnTcpMessage(std::move(request_message));
                }
            });

        }

        /**
         * Intercepts all messages sent to the eventbus TCP bridge
         *
         * @param conn - connection
         * @param msg - send message to bridge
         */
        void onMessage(const evpp::TCPConnPtr& conn, evpp::Buffer* msg) {
            std::string request = msg->ToString();
            if (request.find("ping") != std::string::npos) {
                conn->Send("pong", 4);
                msg->Reset();
                return;
            }

            _eventBusThreadLocal->GetNextLoop()->QueueInLoop([this, request = std::move(request)] () {
                ClusteredMessage request_message = to_message(request, false);
                processOnTcpMessage(std::move(request_message));
            });


            conn->Send("", 0);
            msg->Reset();

        }

    private:

        void processOnTcpMessage (ClusteredMessage&& request_message) {
            std::string addr = request_message.getHost() + ":" + fmt::format_int(request_message.getPort()).str();
            // jeśli wiadomość jest zapytaniem z request
            if (request_message.isRequest()) {
                if (request_message.isLocal()) {
                    request_message.getFunc()(request_message);
                    request_message.body(std::move(request_message.reply()));
                    request_message.getCallbackFunc()(request_message);
                    return ;
                }

                // jeśli wiadomość jest na tego samego vertx
                if (request_message.getHost() == options.getHost() && request_message.getPort() == options.getPort()) {
                    auto function_invoke = _consumers[request_message.getReplay()];
                    function_invoke(request_message);
                    {
                        std::unique_lock<std::mutex> lock(_resp_mutex);
                        auto it = _publishers.find(request_message.getAddress());
                        if (it != _publishers.cend()) {
                            request_message.body(std::move(request_message.reply()));
                            it->second(request_message);
                            _publishers.erase(it);
                        }
                    }
                    return;
                }

                // jeśli jest na innego vertx
                ClusteredMessage msg = request_message;
                msg.setPort(options.getPort());
                msg.setHost(options.getHost());
                std::string message_str = to_string(msg);
                processStringMessage(std::move(message_str), std::move(addr));
                return;
            }

            auto address = request_message.getAddress();
            if (address.find("__vertx.reply") != std::string::npos) {
                {
                    std::unique_lock<std::mutex> lock(_resp_mutex);
                    auto reposne_invoke = _publishers[address];
                    reposne_invoke(request_message);
                    _publishers.erase(address);
                }

                return;
            }

            auto function_invoke = _consumers[request_message.getAddress()];

            ClusteredMessage response_message{0, 1, 9, true, request_message.getAddress(), request_message.getReplay(), options.getPort(), request_message.getHost(), 4, ""};
            function_invoke(request_message);

            std::string message_str = to_string(response_message);
            processStringMessage(std::move(message_str), std::move(addr));
        }

        /**
         * Send message to consumer or sender via tcp
         *
         * @param message_str - message to send
         * @param addr - tcp address
         */
        void processStringMessage (const std::string&& message_str, const std::string&& addr) {
            auto h = std::hash<std::string>()(addr);
            if (auto it = endpoints.find(h); it == endpoints.end()) {
                auto f = std::async(std::launch::async,[this, h, addr = std::move(addr), message_str = std::move(message_str)]() {
                    evpp::EventLoop eb;

                    std::shared_ptr<evpp::TCPClient> client = std::make_shared<evpp::TCPClient>(&eb, addr,"tcp-conn-" + addr);
                    client->SetConnectionCallback([this, client, h, message_str = std::move(message_str)](const evpp::TCPConnPtr &conn) {
                        if (conn->IsConnected()) {
                            LOG_INFO << "connected to " << conn->remote_addr();
                            conn->Send(message_str);
                        }
                    });
                    client->Connect();
                    client->set_auto_reconnect(false);
                    endpoints.emplace(h, client);
                    eb.Run();
                });
                evConnect.push_back(std::move(f));
            } else {
                it->second->conn()->Send(message_str);
            }
        }

        /**
         * Init eventloop for eventbus tcp bridge
         */
        void allocateEventLoop() {
            LOG_DEBUG << " Init EventLoop... in size: " << options.getEventBusPoolSize();
            auto f = std::async(std::launch::async, [this] () {
                _eventBusPool = std::make_shared<evpp::EventLoop>();
                _eventBusThreadLocal = std::make_shared<evpp::EventLoopThreadPool>(_eventBusPool.get(), options.getEventBusPoolSize());
                _eventBusThreadLocal->Start(true);
                _eventBusPool->Run();
            });
            fs.push_back(std::move(f));

        }
    };


}


#endif //VERTX_CPP_ASIO_EVENT_BUS_HPP
