#include <string>
#include <iostream>
#include <fstream>
#include <csignal>
#include <utility>
#include <vector>
#include <thread>

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/client_context.h>
#include "src/protos/kvmaster.grpc.pb.h"
#include "src/common/utils.h"
#include "src/kv/master_client.h"

using namespace grpc;
using namespace std;


class KVMasterImpl final : public KVMaster::Service, public KVMasterClient {
public:
    string addr;

    explicit KVMasterImpl(const string &addr, vector<MasterPartitionView> &partitions)
            : KVMasterClient(partitions), addr(addr) {}

    // ==== RPC impls ====
    Status GetNodeView(ServerContext *context, const NodeViewRequest *req, NodeView *resp) override {
        for (auto &part: partitions) {
            auto part_resp = resp->add_partition();
            part_resp->set_primary_idx(part.primary_idx);
            for (auto &node: part.nodes) {
                auto node_resp = part_resp->add_node();
                node_resp->set_address(node.addr);
                node_resp->set_alive(node.alive);
            }
        }
        return Status::OK;
    }

    Status
    GetPartitionPrimary(ServerContext *context, const GetPrimaryRequest *req, GetPrimaryResponse *resp) override {
        unsigned idx = req->partition_idx();
        if (idx >= partitions.size())
            return Status(StatusCode::INVALID_ARGUMENT, "partition index out of range");
        auto &partition = partitions[idx];
        if (partition.primary_idx == -1) {
            resp->set_addr("");
            return Status::OK;
        }
        auto &p_addr = partition.nodes[partition.primary_idx].addr;
        resp->set_addr(p_addr);
        for (auto &node: partition.nodes) {
            auto node_resp = resp->add_nodes();
            node_resp->set_address(node.addr);
            node_resp->set_alive(node.alive);
        }
        return Status::OK;
    }

    // ==== healthcheck ====
    void do_healthcheck() {
        // multicast gRPC to each client
        KVMasterClient::do_healthcheck();

        // if the primary of a partition is down or unknown (-1), appoint a new one
        for (int p = 0; p < partitions.size(); p++) {
            auto &part = partitions[p];
            if (part.primary_idx != -1 && part.nodes[part.primary_idx].alive) continue;
            // find the next alive node in the partition and make it the primary
            bool mcast_primary_update = false;
            for (int i = 0; i < part.nodes.size(); i++) {
                auto &node = part.nodes[i];
                if (node.alive) {
                    part.primary_idx = i;
                    mcast_primary_update = true;
                    break;
                }
            }
            if (!mcast_primary_update) {
                // there are no alive nodes in this partition
                logf("[WARN] partition %d has no healthy nodes to appoint new primary\n", p);
                part.primary_idx = -1;
                continue;
            }
            // multicast the update to the partition
            auto &primary_addr = part.nodes[part.primary_idx].addr;
            logf("[INFO] appointing new primary for partition %d: %s\n", p, primary_addr.c_str());
            vector<thread> pri_threads;
            for (auto &node: part.nodes)
                pri_threads.emplace_back(&KVMasterClient::_update_primary, &node, primary_addr);
            for (auto &t: pri_threads)
                t.join();
        }
    }
};

// globals
bool running = true;
unique_ptr<Server> server;

// healthcheck thread
void healthcheck_each_second(KVMasterImpl *service) {
    while (running) {
        service->do_healthcheck();
        sleep(1);
    }
}

void do_shutdown() {
    server->Shutdown();
}

// main entrypoints
void handle_signal(int signo) {
    // call to shutdown must happen in another thread :/
    // https://github.com/grpc/grpc/issues/24884
    thread t(&do_shutdown);
    t.join();
}

int main(int argc, char *argv[]) {
    // ./kvmaster <path to config>
    // ex. bazel-bin/src/kvmaster kvconfig_min.txt
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    string config_path = argv[1];

    // read and parse config file
    vector<MasterPartitionView> partitions;
    string master_addr;
    // open the file (or error if we can't)
    ifstream f;
    f.open(config_path, ios::in);
    if (!f.is_open()) {
        fprintf(stderr, "Could not open file: %s\n", config_path.c_str());
        exit(EXIT_FAILURE);
    }
    // 1st line is the master address
    if (!getline(f, master_addr)) {
        fprintf(stderr, "Config file should have KV master addr on first line\n");
        exit(EXIT_FAILURE);
    }
    // parse each partition line
    string line;
    while (getline(f, line)) {
        cout << "[INIT] Found partition config" << endl;
        vector<MasterNodeView> nodes;
        int i = 0, j = 0;
        while (j != string::npos) {
            j = line.find(',', i);
            auto node_addr = line.substr(i, j == string::npos ? j : j - i);
            nodes.emplace_back(node_addr);
            i = j + 1;
            cout << node_addr << endl;
        }
        partitions.emplace_back(nodes);
    }

    // and close the file
    f.close();

    // register signal handler
    signal(SIGINT, handle_signal);

    // serve master
    KVMasterImpl service = KVMasterImpl(master_addr, partitions);

    ServerBuilder builder;
    builder.AddListeningPort(master_addr, InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    cout << "[INIT] Server listening on " << master_addr << endl;
    cout << "serving " << service.partitions.size() << " partitions" << endl;

    // launch threads
    thread pinger(healthcheck_each_second, &service);

    // run until ctrl-c
    server->Wait();
    running = false;

    // shutdown
    pinger.join();
}
