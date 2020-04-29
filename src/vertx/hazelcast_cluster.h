//
// Created by nerull on 14.04.2020.
//

#ifndef VERTX_TCP_SEASTAR_HAZELCAST_CLUSTER_H
#define VERTX_TCP_SEASTAR_HAZELCAST_CLUSTER_H
//#include <hazelcast/client/HazelcastAll.h>
#include <hazelcast/client/HazelcastClient.h>
#include <hazelcast/client/MembershipListener.h>
#include <hazelcast/client/InitialMembershipListener.h>
#include <hazelcast/client/InitialMembershipEvent.h>
#include <hazelcast/client/MembershipEvent.h>
#include <hazelcast/client/LifecycleListener.h>
#include <hazelcast/client/serialization/ObjectDataInput.h>
#include <hazelcast/client/serialization/ObjectDataOutput.h>
#include <evpp/logging.h>
#include <memory>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <queue>
//#include <folly/concurrency/ConcurrentHashMap.h>

using namespace rapidjson;

using haInfo = hazelcast::client::IMap<std::string, std::string>;
using subs = hazelcast::client::MultiMap<std::string, std::string>;

class ServerID {
public:
    ServerID() {};
    ServerID(int port, const std::string &host) : port(port), host(host) {};

    int getPort() const {
        return port;
    }

    const std::string &getHost() const {
        return host;
    }

private:
    int port;
    std::string host;
};

class ClusterNodeInfo {
public:
    ClusterNodeInfo(const std::string &nodeId, const ServerID &serverId) : nodeId(nodeId), serverId(serverId) {}
    ClusterNodeInfo() {};

    static inline ClusterNodeInfo toObject (std::string json) {
        ClusterNodeInfo node;
        Document d;
        d.Parse(json.c_str());
        Value& nodeId = d["nodeId"];
        Value& serverID = d["serverID"];
        Value& port = serverID["port"];
        Value& host = serverID["host"];
        node.nodeId = nodeId.GetString();
        node.serverId = ServerID(port.GetInt(), host.GetString());

        return node;
    }

    const ServerID &getServerId() const {
        return serverId;
    }

private:
    std::string nodeId;
    ServerID serverId;
};

class HaEntryListener: public EntryListener<std::string, std::string> {

public:
    void entryAdded(const EntryEvent<std::string, std::string> &event) override {
        LOG_DEBUG << "HaEntryListener entryAdded: " <<  event.getKey() ;
    }

    void entryRemoved(const EntryEvent<std::string, std::string> &event) override {
        LOG_DEBUG << "HaEntryListener entryRemoved: " <<  event.getKey();
    }

    void entryUpdated(const EntryEvent<std::string, std::string> &event) override {
        LOG_DEBUG << "HaEntryListener entryUpdated: " <<  event.getKey();
    }

    void entryEvicted(const EntryEvent<std::string, std::string> &event) override {
        LOG_DEBUG << "HaEntryListener entryEvicted: " <<  event.getKey();
    }

    void entryExpired(const EntryEvent<std::string, std::string> &event) override {
        LOG_DEBUG << "HaEntryListener entryExpired: " <<  event.getKey();
    }

    void entryMerged(const EntryEvent<std::string, std::string> &event) override {
        LOG_DEBUG << "HaEntryListener entryMerged: " <<  event.getKey();
    }

    void mapEvicted(const MapEvent &event) override {
        LOG_DEBUG << "HaEntryListener mapEvicted: " <<  event.getEventType();
    }

    void mapCleared(const MapEvent &event) override {

    }

};


class hazelcast_cluster: public MembershipListener, LifecycleListener, EntryListener<std::string, std::string>, std::enable_shared_from_this<hazelcast_cluster> {

public:
    hazelcast_cluster(ClientConfig config);

    void join(int port, std::string & host);

    void memberAdded(const MembershipEvent &membershipEvent) override;

    void memberRemoved(const MembershipEvent &membershipEvent) override;

    void memberAttributeChanged(const MemberAttributeEvent &memberAttributeEvent) override;

    virtual ~hazelcast_cluster();

    void addSub (std::string & address, int port, std::string & host);

    ServerID next(std::string);

private:
    void entryAdded(const EntryEvent<std::string, std::string> &event) override;

    void entryRemoved(const EntryEvent<std::string, std::string> &event) override;

    void entryUpdated(const EntryEvent<std::string, std::string> &event) override;

    void entryEvicted(const EntryEvent<std::string, std::string> &event) override;

    void entryExpired(const EntryEvent<std::string, std::string> &event) override;

    void entryMerged(const EntryEvent<std::string, std::string> &event) override;

    void mapEvicted(const MapEvent &event) override;

    void mapCleared(const MapEvent &event) override;

    void stateChanged(const LifecycleEvent &lifecycleEvent) override;

    static constexpr std::string_view _NODE_ID_ATTRIBUTE = "__vertx.nodeId";
    hazelcast::client::ClientConfig _config;
    hazelcast::client::HazelcastClient _hazelcast;
    std::string _nodeID;
    std::string _membershipListenerId;
    std::string _lifecycleListenerId;
    std::shared_ptr<haInfo> _haInfo;
    std::shared_ptr<subs> _subs;
//    SubEntryListener _subListener;
    HaEntryListener _haListener;

    std::unordered_multimap<std::string, std::string> _local_subs;
    std::unordered_map<std::string, std::queue<ServerID>> _local_endpoints;


};


#endif //VERTX_TCP_SEASTAR_HAZELCAST_CLUSTER_H
