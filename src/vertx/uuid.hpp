//
// Created by nerull on 27.04.2020.
//

#ifndef VERTX_CPP_ASIO_UUID_HPP
#define VERTX_CPP_ASIO_UUID_HPP

#include <string>
#include <cstdlib>

namespace uuid {
    const std::string CHARS = "0123456789abcdef";

    std::string generateUUID() {
        std::string uuid = std::string(36,' ');
        uuid[8] = '-';
        uuid[13] = '-';
        uuid[18] = '-';
        uuid[23] = '-';
        for(int i=0;i<36;i++){
            if (i != 8 && i != 13 && i != 18 && i != 23) {
                int idx = std::rand() % CHARS.size();
                uuid[i] = CHARS[idx];
            }
        }
        return uuid;
    }

}


#endif //VERTX_CPP_ASIO_UUID_HPP
