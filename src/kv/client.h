#pragma once

#include <openssl/md5.h>
#include <istream>

#include <grpcpp/grpcpp.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include "src/protos/kv.grpc.pb.h"
#include "src/protos/kvmaster.grpc.pb.h"
#include "src/kv/exceptions.h"
#include "src/common/utils.h"

using namespace grpc;
using namespace std;

class Node {
public:
    string addr;
    bool alive;

    Node(const string &addr, bool alive) : addr(addr), alive(alive) {}
};

class Partition {
public:
    vector<Node> nodes;
    unique_ptr<KVStore::Stub> stub;  // round-robin stub that talks to any node in channel

    explicit Partition(vector<Node> &n);
};

class KVClient {
    unique_ptr<KVMaster::Stub> master_stub;
    vector<Partition> partitions;

    Partition &row_partition(const string &row);
    void refresh_partitions();

public:
    explicit KVClient(const string &master_addr);
    int put(const string &row, const string &col, const string &data);
    string get(const string &row, const string &col);
    string getdefault(const string &row, const string &col, const string &def_val);
    int cput(const string &row, const string &col, const string &v1, const string &v2);
    int del(const string &row, const string &col);
};
