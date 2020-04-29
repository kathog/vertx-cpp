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
#include "clustered_message.h"
#include "uuid.hpp"
#include <evpp/buffer.h>
#include <evpp/tcp_conn.h>
#include <future>
#include <evpp/tcp_client.h>
#include <moodycamel/concurrentqueue.h>
#include <moodycamel/blockingconcurrentqueue.h>


using namespace std::chrono_literals;

namespace eventbus {



    typedef std::function<void(const clustered_message&)> MsgCallback;
    typedef std::function<void(const clustered_message&, clustered_message&)> RequestMsgCallback;

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
        std::unordered_map<std::string, RequestMsgCallback> _consumers;
        std::unordered_map<std::string, MsgCallback> _publishers;
        std::vector<std::future<void>> evConnect;
        std::unordered_map<long, std::shared_ptr<evpp::TCPClient>> endpoints;
        std::vector<std::future<void>> fs;
//        std::queue<clustered_message> _messages;

//        moodycamel::ConcurrentQueue<clustered_message> _messages;

//        std::mutex _queue_mutex;
        std::mutex _resp_mutex;
//        std::condition_variable _condition;

    public:
        EventBus(std::shared_ptr<hazelcast_cluster> hz, EventBusOptions& options) : hz(hz), options(options) {
            allocateEventLoop();
        }

        /**
         * Dodanie konsumera do eventbus
         *
         * @param address - adres na jakim na nasłuchiwać
         * @param function - funkcjia do wywołania
         */
        void consumer (std::string&& address, RequestMsgCallback function) {
            _consumers.emplace(address, function);
            hz->addSub(address, options.getPort(), const_cast<std::string &>(options.getHost()));
        }

        /**
         * Wysłanie zapytania do consumera za pomocą eventbus
         *
         * @param address - adres konsumera
         * @param value - wartość do przesłania
         * @param func - funkcja wywołana po otrzymaniu odpowiedzi
         */
        void request (std::string&& address, std::any value, const MsgCallback& func) {
            std::string uuid_ = "__vertx.reply." + uuid::generateUUID();
            ServerID server_ = hz->next(address);

            {
                std::unique_lock<std::mutex> lock(_resp_mutex);
                _publishers.emplace(uuid_, const_cast<MsgCallback&>(func));
            }
            clustered_message request_message = clustered_message{0, 1, 9, true, uuid_, address, server_.getPort(), server_.getHost(), 4, value};
            request_message.setRequest(true);

            processOnTcpMessage(std::move(request_message));
        }

        /**
         * Przechwytuje wszystkie zdarzenia przesłane do danej instancji vertx
         *
         * @param conn - połączenie
         * @param msg - odebana wiadomość
         */
        void onMessage(const evpp::TCPConnPtr& conn, evpp::Buffer* msg) {
            std::string request = msg->ToString();
            if (request.find("ping") != std::string::npos) {
                conn->Send("pong", 4);
                msg->Reset();
                return;
            }

            _eventBusThreadLocal->GetNextLoop()->QueueInLoop([this, request = std::move(request)] () {
                clustered_message request_message = to_message(request, false);
                processOnTcpMessage(std::move(request_message));
            });


            conn->Send("", 0);
            msg->Reset();

        }

    private:

        void processOnTcpMessage (clustered_message&& request_message) {
            std::string addr = request_message.getHost() + ":" +std::to_string(request_message.getPort());

            // jeśli wiadomość jest zapytaniem z request
            if (request_message.isRequest()) {
                _eventBusThreadLocal->GetNextLoop()->QueueInLoop([this, request_message = std::move(request_message), addr = std::move(addr)] () {

                    // jeśli wiadomość jest na tego samego vertx
                    if (request_message.getHost() == options.getHost() && request_message.getPort() == options.getPort()) {
                        auto function_invoke = _consumers[request_message.getReplay()];
                        clustered_message response_message{0, 1, 9, true, request_message.getAddress(), request_message.getReplay(),options.getPort(), request_message.getHost(), 4, ""};
                        clustered_message msg = request_message;
                        function_invoke(request_message, msg);
                        {
                            std::unique_lock<std::mutex> lock(_resp_mutex);
                            auto reposne_invoke = _publishers[request_message.getAddress()];
                            reposne_invoke(msg);
                            _publishers.erase(request_message.getAddress());
                        }
                        return;
                    }

                    // jeśli jest na innego vertx
                    clustered_message msg = request_message;
                    msg.setPort(options.getPort());
                    msg.setHost(options.getHost());
                    std::string message_str = to_string(msg);

                    _eventBusThreadLocal->GetNextLoop()->QueueInLoop([this, message_str = std::move(message_str), addr = std::move(addr)] () {
                        processStringMessage(std::move(message_str), std::move(addr));
                    });

                });
                return;
            }

            auto address = request_message.getAddress();
            if (address.find("__vertx.reply") != std::string::npos) {
                {
                    std::unique_lock<std::mutex> lock(_resp_mutex);
                    auto it = _publishers.find(address);
                    if (it != _publishers.end()) {
                        it->second(request_message);
                        _publishers.erase(address);
                    }
                }

                return;
            }

            auto function_invoke = _consumers[request_message.getAddress()];

            _eventBusThreadLocal->GetNextLoop()->QueueInLoop([this, function_invoke, request_message = std::move(request_message), addr = std::move(addr)] () {
                clustered_message response_message{0, 1, 9, true, request_message.getAddress(), request_message.getReplay(),options.getPort(), request_message.getHost(), 4, ""};
                function_invoke(request_message, response_message);

                std::string message_str = to_string(response_message);
                _eventBusThreadLocal->GetNextLoop()->QueueInLoop([this, message_str = std::move(message_str), addr = std::move(addr)] () {
                    processStringMessage(std::move(message_str), std::move(addr));
                });
            });

        }

        /**
         * Wysłanie wiadomości po TCP
         *
         * @param message_str
         * @param addr
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
         * Inicjalizacja evenloop obsługująca całe flow przetwarzania komunikatów na eventbus
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
