#pragma once

#include "src/kv/master_client.h"
#include "src/protos/kv.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using namespace std;

class AdminConsole : public KVMasterClient {
private:
//    std::vector<MasterPartitionView> partitions;
    // vector<pair<string, bool>> backend_status; // (node address, is_alive)
    // vector<pair<string, bool>> frontend_status; // (node address, is_alive)

public:
    AdminConsole(vector<MasterPartitionView>& partitions) : KVMasterClient(partitions) {}
	//std::vector<bool> check_backend_status();
	std::vector<bool> check_frontend_status();
    std::vector<std::string> get_frontend_nodes();
    //std::vector<std::string> get_backend_nodes();
    bool disable_frontend(const std::string& node);
//    bool disable_backend(const std::string& node);
    bool restart_frontend(const std::string& node);
    string generate_response(const string& status_code, const unordered_map<string, string>& headers, const string& content);
//    bool restart_backend(const std::string& node);

};
