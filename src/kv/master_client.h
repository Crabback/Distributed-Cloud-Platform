#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include "src/protos/kv.grpc.pb.h"
#include "src/common/utils.h"
#include "src/kv/exceptions.h"

using namespace grpc;
using namespace std;

class MasterNodeView {
public:
    string addr;
    bool alive;
    unique_ptr<KVStore::Stub> stub;

    explicit MasterNodeView(const string &addr);
};

class MasterPartitionView {
public:
    int primary_idx;
    vector<MasterNodeView> nodes;

    explicit MasterPartitionView(vector<MasterNodeView> &nodes);
};

class KVMasterClient {
public:
    vector<MasterPartitionView> partitions;

    explicit KVMasterClient(vector<MasterPartitionView> &partitions);
//    void do_healthcheck();
//    void do_healthcheck(vector<unordered_map<string, bool> > &node_status);
    vector<bool> do_healthcheck();
    bool adminshutdown(const std::string& node);
    bool adminstartup(const std::string& node);
    vector<pair<string, string> > displayAdminList();

protected:
    static bool _healthcheck_node(MasterNodeView *node);
    static void _update_primary(MasterNodeView *node, const string &p_addr);

};
