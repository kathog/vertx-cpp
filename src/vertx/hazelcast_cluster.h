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
class ClusterNodeInfo;

using haInfo = hazelcast::client::IMap<std::string, std::string>;
using subs = hazelcast::client::MultiMap<std::string, ClusterNodeInfo>;

class ServerID : public hazelcast::client::serialization::IdentifiedDataSerializable {
public:
    ServerID() {};
    ServerID(int port, const std::string &host) : port(port), host(host) {};

    int getPort() const {
        return port;
    }

    const std::string &getHost() const {
        return host;
    }

    int getFactoryId() const override {
        return 1002;
    }

    int getClassId() const override {
        return 1002;
    }

    void writeData(serialization::ObjectDataOutput &writer) const override {
        writer.writeInt(port);
        writer.writeUTF(&host);

    }

    void readData(serialization::ObjectDataInput &reader) override {
        port = reader.readInt();
        host = *reader.readUTF();
    }

private:
    int port;
    std::string host;
};

class ClusterNodeInfo : public hazelcast::client::serialization::IdentifiedDataSerializable {
public:
    ClusterNodeInfo(const std::string &nodeId, const ServerID &serverId) : nodeId(nodeId), serverId(serverId) {}
    ClusterNodeInfo() {};

    ~ClusterNodeInfo() {};

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

    int getFactoryId() const override {
        return 1001;
    }

    int getClassId() const override {
        return 1001;
    }

    void writeData(serialization::ObjectDataOutput &writer) const override {
        writer.writeUTF(&nodeId);
        writer.writeInt(serverId.getPort());
        writer.writeUTF(&serverId.getHost());
    }

    void readData(serialization::ObjectDataInput &reader) override {
        nodeId = *reader.readUTF();
        int port = reader.readInt();
        std::string host = *reader.readUTF();
        serverId = {port, host};
    }

private:
    std::string nodeId;
    ServerID serverId;
};


class ClusterNodeInfoFactory : public  hazelcast::client::serialization::DataSerializableFactory {
    std::auto_ptr<ClusterNodeInfo::IdentifiedDataSerializable> create(int32_t classId) override {
        return std::auto_ptr<ClusterNodeInfo>(new ClusterNodeInfo());
    }
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


class hazelcast_cluster: public MembershipListener, LifecycleListener, EntryListener<std::string, ClusterNodeInfo>, std::enable_shared_from_this<hazelcast_cluster> {

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
    void entryAdded(const EntryEvent<std::string, ClusterNodeInfo> &event) override;

    void entryRemoved(const EntryEvent<std::string, ClusterNodeInfo> &event) override;

    void entryUpdated(const EntryEvent<std::string, ClusterNodeInfo> &event) override;

    void entryEvicted(const EntryEvent<std::string, ClusterNodeInfo> &event) override;

    void entryExpired(const EntryEvent<std::string, ClusterNodeInfo> &event) override;

    void entryMerged(const EntryEvent<std::string, ClusterNodeInfo> &event) override;

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

    std::unordered_multimap<std::string, ClusterNodeInfo> _local_subs;
    std::unordered_map<std::string, std::queue<ServerID>> _local_endpoints;


};


#endif //VERTX_TCP_SEASTAR_HAZELCAST_CLUSTER_H
