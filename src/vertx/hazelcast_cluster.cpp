//
// Created by nerull on 14.04.2020.
//

#include "hazelcast_cluster.h"
#include <memory>
#include <thread>
#include <mutex>

using namespace std::chrono_literals;

std::mutex _queue_mutex;

hazelcast_cluster::hazelcast_cluster(hazelcast::client::ClientConfig config) : _config(std::move(config)), _hazelcast(_config) {

}
//__vertx.haInfo
//__vertx.subs
void hazelcast_cluster::join(int port, std::string & host) {
    _membershipListenerId = _hazelcast.getCluster().addMembershipListener(boost::shared_ptr<hazelcast_cluster>(this));
    _hazelcast.getLifecycleService().addLifecycleListener(this);
    _nodeID = *_hazelcast.getLocalEndpoint().getUuid();

    _haInfo = std::make_shared<haInfo>(_hazelcast.getMap<std::string, std::string>("__vertx.haInfo"));
    for (auto & [key, value] : _haInfo->entrySet()) {
    }

    _subs = std::make_shared<subs> (_hazelcast.getMultiMap<std::string, std::string>("__vertx.subs"));
    for (auto & [key, value] : _subs->entrySet()) {
        ClusterNodeInfo node = ClusterNodeInfo::toObject(value);
        std::queue<ServerID> queue_;
        queue_.push(node.getServerId());
        if (auto [key_, value] = _local_endpoints.try_emplace(key, queue_); value == false) {
            key_->second.push(node.getServerId());
        }
    }

    _subs->addEntryListener(*this, true);
    _haListener = {};
    _haInfo->addEntryListener(_haListener, true);

    _haInfo->put(_nodeID, "{\"verticles\":[],\"group\":\"__DISABLED__\",\"server_id\":{\"host\":\"" + host +"\",\"port\":" + std::to_string(port) + "}}");

    std::this_thread::sleep_for(1s);
}

void hazelcast_cluster::addSub (std::string & address, int port, std::string & host) {
    _subs->put(address, "{\"nodeId\":\""+ _nodeID +"\",\"serverID\":{\"port\":" + std::to_string(port) + ",\"host\":\""+host+"\"}}");
    _local_subs.emplace(address, "{\"nodeId\":\""+ _nodeID +"\",\"serverID\":{\"port\":" + std::to_string(port) + ",\"host\":\""+host+"\"}}");
}

void hazelcast_cluster::memberAdded(const MembershipEvent &membershipEvent) {
    LOG_DEBUG << "MembershipEvent memberAdded: " << membershipEvent.getMember().getUuid();
}

void hazelcast_cluster::memberRemoved(const MembershipEvent &membershipEvent) {
    LOG_DEBUG << "MembershipEvent memberRemoved: " << membershipEvent.getMember().getUuid();
}

void hazelcast_cluster::memberAttributeChanged(const MemberAttributeEvent &memberAttributeEvent) {

}

void hazelcast_cluster::stateChanged(const LifecycleEvent &lifecycleEvent) {
    LOG_DEBUG << "stateChanged: " << lifecycleEvent.getState() ;
}


void hazelcast_cluster::entryAdded(const EntryEvent<std::string, std::string> &event) {
    LOG_DEBUG << "SubEntryListener entryAdded: " << event.getKey();
    LOG_DEBUG << event.getValue();
    std::unique_lock<std::mutex> lock(_queue_mutex);
    ClusterNodeInfo node = ClusterNodeInfo::toObject(event.getValue());
    std::queue<ServerID> queue_;
    queue_.push(node.getServerId());
    if (auto [key, value] = _local_endpoints.try_emplace(event.getKey(), queue_); value == false) {
        key->second.push(node.getServerId());
    }
}



ServerID hazelcast_cluster::next(std::string address) {
    std::unique_lock<std::mutex> lock(_queue_mutex);
    std::queue<ServerID> queue_ = std::move(_local_endpoints.at(address));
    ServerID id = std::move(queue_.front());
    queue_.pop();
    queue_.push(id);
    _local_endpoints.at(address) = std::move(queue_);
    return id;
}

void hazelcast_cluster::entryRemoved(const EntryEvent<std::string, std::string> &event) {
    LOG_DEBUG << "SubEntryListener entryRemoved: " << event.getKey();
    std::unique_lock<std::mutex> lock(_queue_mutex);
    _local_endpoints.erase(event.getKey());

    for (auto& valueNode : _subs->get(event.getKey())) {
        ClusterNodeInfo node = ClusterNodeInfo::toObject(valueNode);
        std::queue<ServerID> queue_;
        queue_.push(node.getServerId());
        if (auto [key, value] = _local_endpoints.try_emplace(event.getKey(), queue_); value == false) {
            key->second.push(node.getServerId());
        }
    }

    for (auto&[key, value] : _local_subs) {
        _subs->put(key, value);
    }
}

void hazelcast_cluster::entryUpdated(const EntryEvent<std::string, std::string> &event) {
    LOG_DEBUG << "SubEntryListener entryUpdated: " << event.getKey();
}

void hazelcast_cluster::entryEvicted(const EntryEvent<std::string, std::string> &event) {
    LOG_DEBUG << "SubEntryListener entryEvicted: " << event.getKey();
}

void hazelcast_cluster::entryExpired(const EntryEvent<std::string, std::string> &event) {

}

void hazelcast_cluster::entryMerged(const EntryEvent<std::string, std::string> &event) {

}

void hazelcast_cluster::mapEvicted(const MapEvent &event) {

}

void hazelcast_cluster::mapCleared(const MapEvent &event) {

}

hazelcast_cluster::~hazelcast_cluster() {
}
