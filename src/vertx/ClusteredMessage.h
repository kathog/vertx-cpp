//
// Created by nerull on 18.04.2020.
//

#ifndef VERTX_TCP_SEASTAR_CLUSTERED_MESSAGE_H
#define VERTX_TCP_SEASTAR_CLUSTERED_MESSAGE_H

#include <string>
#include <ostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <any>

class ClusteredMessage;

//typedef std::function<void(const ClusteredMessage&, ClusteredMessage&)> RequestMsgCallback;
typedef std::function<void(ClusteredMessage&)> MsgCallback;

class ClusteredMessage {

public:

    inline ClusteredMessage to_message (std::string& msg) {
        const char * b_data = msg.c_str();
        int index = 0;
        ClusteredMessage message {int_value(b_data, index),
                                  (int) b_data[index++],
                                  (int) b_data[index++],
                                  (((int) b_data[index++]) == 0),
                                  string_value(b_data, index),
                                  string_value(b_data, index),
                                  int_value(b_data, index),
                                  string_value(b_data, index),
                                  int_value(b_data, index),
                                  string_value(b_data, index)
                                   };
        return std::move(message);
    }

    ClusteredMessage to_message (std::string&& msg) {
        const char * b_data = msg.c_str();
        int index = 0;
        return {
            int_value(b_data, index),
            (int) b_data[index++],
            (int) b_data[index++],
            (((int) b_data[index++]) == 0),
            string_value(b_data, index),
            string_value(b_data, index),
            int_value(b_data, index),
            string_value(b_data, index),
            int_value(b_data, index),
            string_value(b_data, index)
        };
    }

    void to_cstring (char* result, int& index) {
        set_int(result, index, message_size); //size
        result[index++] = static_cast<char>(1); // protocol version
        result[index++] = static_cast<char>(9); // system codec, string
        result[index++] = static_cast<char>(0); // send
        set_int(result, index, replay.size()); //reply
        for (char c : replay) {
            result[index++] = c;
        }
        set_int(result, index, address.size()); // replay address
        for (char c : address) {
            result[index++] = c;
        }
        set_int(result, index, port); // port
        set_int(result, index, host.size()); //host
        for (char c : host) {
            result[index++] = c;
        }
        set_int(result, index, 4); // headers
        if (_reply.type() == typeid(std::string)) {
            std::string s_body = getReplyAsString();
            set_int(result, index, s_body.size()); // body
            for (char c : s_body) {
                result[index++] = c;
            }
        }
        set_int(result, 0, index - 4);
    }

    ClusteredMessage() {}

//    ClusteredMessage(const ClusteredMessage& msg) = delete;

    ClusteredMessage(int messageSize, int protocolVersion, int systemCodecId, bool send, const std::string &address,
                     const std::string &replay, int port, const std::string &host, int headers,
                     const std::any &body) : message_size(messageSize), protocol_version(protocolVersion),
                                                 system_codec_id(systemCodecId), send(send), address(address),
                                                 replay(replay), port(port), host(host), headers(headers), body(body) {}

    friend std::ostream &operator<<(std::ostream &os, const ClusteredMessage &message) {
        os << "message_size: " << message.message_size << "\n protocol_version: " << message.protocol_version
           << "\n system_codec_id: " << message.system_codec_id << "\n send: " << message.send << "\n address: "
           << message.address << "\n replay: " << message.replay << "\n port: " << message.port << "\n host: " << message.host
           << "\n headers: " << message.headers << "\n body: " << std::any_cast<std::string>(message.body);
        return os;
    }

    const std::any &getReply() const {
        return _reply;
    }

    void setReply(const std::any &reply)  {
        _reply = reply;
    }

    void setPort(int port) {
        ClusteredMessage::port = port;
    }

    void setHost(const std::string &host) {
        ClusteredMessage::host = host;
    }

    void setBody(const std::any &body) {
        this->body = body;
    }

    int getMessageSize() const {
        return message_size;
    }

    int getProtocolVersion() const {
        return protocol_version;
    }

    int getSystemCodecId() const {
        return system_codec_id;
    }

    bool isSend() const {
        return send;
    }

    const std::string &getAddress() const {
        return address;
    }

    const std::string &getReplay() const {
        return replay;
    }

    int getPort() const {
        return port;
    }

    const std::string &getHost() const {
        return host;
    }

    int getHeaders() const {
        return headers;
    }

    const std::any &getBody() const {
        return body;
    }

    const std::string getBodyAsString () {
        return std::any_cast<std::string>(body);
    }

    const std::string getReplyAsString () {
        return std::any_cast<std::string>(_reply);
    }

    bool isRequest() const {
        return request;
    }

    void setRequest(bool request) {
        this->request = request;
    }

    const MsgCallback &getFunc() const {
        return func;
    }

    const MsgCallback &getCallbackFunc() const {
        return callbackFunc;
    }

    void setFunc(const MsgCallback &func, const MsgCallback& callbackFunc) {
        this->func = func;
        this->callbackFunc = callbackFunc;
        this->local = true;
    }

    bool isLocal() const {
        return local;
    }

private:
    inline static int int_value (const char * value, int& startIdx) {
        return (( (value[startIdx++] & 0xff) << 24) |( (value[startIdx++] & 0xff) << 16) |( (value[startIdx++] & 0xff) << 8) | ( (value[startIdx++] & 0xff)));
    }

    inline static void set_int (char * value, int& startIdx, int& i_value) {
        value[startIdx++] = (char) ((i_value & 0xff000000) >> 24);
        value[startIdx++] = (char) ((i_value & 0x00ff0000) >> 16);
        value[startIdx++] = (char) ((i_value & 0x0000ff00) >> 8);
        value[startIdx++] = (char) ((i_value & 0x000000ff) );
    }

    inline static void set_int (char * value, int& startIdx, int&& i_value) {
        value[startIdx++] = (char) ((i_value & 0xff000000) >> 24);
        value[startIdx++] = (char) ((i_value & 0x00ff0000) >> 16);
        value[startIdx++] = (char) ((i_value & 0x0000ff00) >> 8);
        value[startIdx++] = (char) ((i_value & 0x000000ff) );
    }

    inline static void set_int (char * value, int&& startIdx, int&& i_value) {
        value[startIdx++] = (char) ((i_value & 0xff000000) >> 24);
        value[startIdx++] = (char) ((i_value & 0x00ff0000) >> 16);
        value[startIdx++] = (char) ((i_value & 0x0000ff00) >> 8);
        value[startIdx++] = (char) ((i_value & 0x000000ff) );
    }

    inline static void set_int (std::string& value, int& startIdx, int& i_value) {
        value[startIdx++] = (char) ((i_value & 0xff000000) >> 24);
        value[startIdx++] = (char) ((i_value & 0x00ff0000) >> 16);
        value[startIdx++] = (char) ((i_value & 0x0000ff00) >> 8);
        value[startIdx++] = (char) ((i_value & 0x000000ff) );
    }

    inline static void set_int (std::string& value, int& startIdx, int&& i_value) {
        value[startIdx++] = (char) ((i_value & 0xff000000) >> 24);
        value[startIdx++] = (char) ((i_value & 0x00ff0000) >> 16);
        value[startIdx++] = (char) ((i_value & 0x0000ff00) >> 8);
        value[startIdx++] = (char) ((i_value & 0x000000ff) );
    }

    inline static void set_int (std::string& value, int&& startIdx, int&& i_value) {
        value[startIdx++] = (char) ((i_value & 0xff000000) >> 24);
        value[startIdx++] = (char) ((i_value & 0x00ff0000) >> 16);
        value[startIdx++] = (char) ((i_value & 0x0000ff00) >> 8);
        value[startIdx++] = (char) ((i_value & 0x000000ff) );
    }

    inline static std::string string_value (const char * value, int& startIdx) {
        int size = int_value(value, startIdx);
        std::stringstream s;
        if (size > 0) {
            size = startIdx + size;
            while (startIdx < size ) {
                s << value[startIdx++];
            }
        }
        return s.str();
    }


    int message_size;
    int protocol_version;
    int system_codec_id;
    bool send;
    std::string address;
    std::string replay;
    int port;
    std::string host;
    int headers;
    std::any body;
    mutable std::any _reply;
    bool request;
    MsgCallback func;
    MsgCallback callbackFunc;
    bool local = false;
};

inline int int_value (const char * value, int& startIdx) {
    return (( (value[startIdx++] & 0xff) << 24) |( (value[startIdx++] & 0xff) << 16) |( (value[startIdx++] & 0xff) << 8) | ( (value[startIdx++] & 0xff)));
}

inline std::string string_value (const char * value, int& startIdx) {
    int size = int_value(value, startIdx);
    std::stringstream s;
    if (size > 0) {
        size = startIdx + size;
        while (startIdx < size ) {
            s << value[startIdx++];
        }
    }
    return s.str();
}

inline ClusteredMessage to_message (const std::string& msg, bool request) {
    const char * b_data = msg.c_str();
    int index = 0;

    ClusteredMessage message = {
            int_value(b_data, index),
            (int) b_data[index++],
            (int) b_data[index++],
            (((int) b_data[index++]) == 0),
            string_value(b_data, index),
            string_value(b_data, index),
            int_value(b_data, index),
            string_value(b_data, index),
            int_value(b_data, index),
            string_value(b_data, index)
    };
    message.setRequest(request);

    return std::move(message);
}

inline ClusteredMessage to_message (std::string&& msg, bool request) {
    const char * b_data = msg.c_str();
    int index = 0;

    ClusteredMessage message = {
            int_value(b_data, index),
            (int) b_data[index++],
            (int) b_data[index++],
            (((int) b_data[index++]) == 0),
            string_value(b_data, index),
            string_value(b_data, index),
            int_value(b_data, index),
            string_value(b_data, index),
            int_value(b_data, index),
            string_value(b_data, index)
    };
    message.setRequest(request);

    return std::move(message);
}

inline ClusteredMessage to_message (std::string&& msg) {
    const char * b_data = msg.c_str();
    int index = 0;

    ClusteredMessage message = {
            int_value(b_data, index),
            (int) b_data[index++],
            (int) b_data[index++],
            (((int) b_data[index++]) == 0),
            string_value(b_data, index),
            string_value(b_data, index),
            int_value(b_data, index),
            string_value(b_data, index),
            int_value(b_data, index),
            string_value(b_data, index)
    };

    return std::move(message);
}


inline std::string to_string(ClusteredMessage& message) {
    std::string s_message;
    s_message.resize(2048);
    int index = 0;
    message.to_cstring(s_message.data(), index);
    s_message.resize(index);

    return std::move(s_message);
}


#endif //VERTX_TCP_SEASTAR_CLUSTERED_MESSAGE_H
