//
// Created by nerull on 03.05.2020.
//

#ifndef VERTX_CPP_HTTP_SERVER_HPP
#define VERTX_CPP_HTTP_SERVER_HPP
#include <evpp/http/http_server.h>
#include <string>

namespace http {

    struct HttpServerOptions {
    public:

        HttpServerOptions& setPoolSize(int size) {
            poolSize = size;
            return *this;
        }

        int getPoolSize() const {
            return poolSize;
        }

    private:
        int poolSize = 1;

    };


    class HttpServer {

    public:
        HttpServer(const HttpServerOptions &options = {}) : _options(options) {
            _server = std::make_shared<evpp::http::Server>(_options.getPoolSize());
        }

        HttpServer& addRoute(std::string path, evpp::http::HTTPRequestCallback callback) {
            _server->RegisterHandler(path, callback);
            return *this;
        }

        HttpServer& addDefaultRoute(evpp::http::HTTPRequestCallback callback) {
            _server->RegisterDefaultHandler(callback);
            return *this;
        }

        void listen(int port) {
            _server->Init(port);
            _server->Start();
        }

    private:
        HttpServerOptions _options;
        std::shared_ptr<evpp::http::Server> _server;

    };


}


#endif //VERTX_CPP_HTTP_SERVER_HPP
