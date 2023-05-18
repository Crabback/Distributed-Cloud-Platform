#include "master_client.h"

MasterNodeView::MasterNodeView(const string &addr) : addr(addr), alive(false) {
    ChannelArguments args;
    args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 100);  // we want healthchecks to always attempt connect
    auto channel = CreateCustomChannel(addr, InsecureChannelCredentials(), args);
    stub = KVStore::NewStub(channel);
}


MasterPartitionView::MasterPartitionView(vector<MasterNodeView> &nodes) : primary_idx(-1), nodes(std::move(nodes)) {}

KVMasterClient::KVMasterClient(vector<MasterPartitionView> &partitions) : partitions(std::move(partitions)) {}

// ==== healthcheck ====
/**
void KVMasterClient::do_healthcheck() {
    // multicast gRPC to each client
    vector<thread> threads;
    for (auto &part: partitions) {
        for (auto &node: part.nodes) {
            threads.emplace_back(&KVMasterClient::_healthcheck_node, &node);
        }
    }

    for (auto &t: threads)
        t.join();
}**/

//void KVMasterClient::do_healthcheck(vector<unordered_map<string, bool> > &node_status ) {
//    // multicast gRPC to each client
//    vector<thread> threads;
//    for (auto &part: partitions) {
//        for (auto &node: part.nodes) {
//            threads.emplace_back(&KVMasterClient::_healthcheck_node, &node);
//            node_status.emplace_back(&KVMasterClient::_healthcheck_node, &node);
//        }
//    }
//
//    for (auto &t: threads)
//        t.join();
//}

vector<bool> KVMasterClient::do_healthcheck() {
    // multicast gRPC to each client
	vector<bool> node_status;
    vector<thread> threads;
    for (auto &part: partitions) {
        for (auto &node: part.nodes) {
            threads.emplace_back(&KVMasterClient::_healthcheck_node, &node);
            node_status.emplace_back(node.alive);
            std::cout<<"Node Address: "<<&node<<"  Node  status: "<<node.alive<<std::endl;
        }
    }

    for (auto &t: threads)
    	t.join();

    return node_status;
}

// multicast inners
bool KVMasterClient::_healthcheck_node(MasterNodeView *node) {
    ClientContext ctx;
    HealthRequest req;
    HealthResponse resp;
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(100));
    Status status = node->stub->HealthCheck(&ctx, req, &resp);
    node->alive = status.ok();
    if (DEBUG)
        logf("[DBG] healthcheck(%s): %d\n", node->addr.c_str(), node->alive);
    return node->alive;
}

void KVMasterClient::_update_primary(MasterNodeView *node, const string &p_addr) {
    ClientContext ctx;
    PrimaryRequest req;
    PrimaryResponse resp;
    req.set_addr(p_addr);
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(100));
    Status status = node->stub->PrimaryUpdate(&ctx, req, &resp);
    if (DEBUG) {
        logf("[DBG] update_primary(%s): %s\n", node->addr.c_str(),
             grpc_status_to_string(status.error_code()).c_str());
    }
}


bool KVMasterClient::adminshutdown(const std::string& node) {
	ShutdownRequest request;
	ShutdownResponse response;
	ClientContext context;
//	MasterNodeView target_node("localhost:1234");
	Status status;
	for (auto &partition : partitions) {
		for (auto &partition_node : partition.nodes) {
			if (node == partition_node.addr) {
//				target_node = std::move(partition_node);
				status = partition_node.stub->AdminShutdown(&context, request, &response);
				break;
			}
		}
	}
//	Status status = target_node.stub->AdminShutdown(&context, request, &response);
	if (status.ok()) {
		return true;
	}
	else {
		return false;
	}
}

bool KVMasterClient::adminstartup(const std::string& node) {
	StartupRequest request;
	StartupResponse response;
	ClientContext context;
//	MasterNodeView target_node("localhost:1234");
	Status status;
	for (auto &partition : partitions) {
		for (auto &partition_node : partition.nodes) {
			if (node == partition_node.addr) {
//				target_node = std::move(partition_node);
				status = partition_node.stub->AdminStartup(&context, request, &response);
				break;
			}
		}
	}
//	Status status = target_node.stub->AdminStartup(&context, request, &response);
	if (status.ok()) {
		return true;
	}
	else {
		return false;
	}
}


vector<pair<string, string> > KVMasterClient::displayAdminList() {
    // Create a ServerContext, ListRequest, and ListResponse object
    vector<pair<string, string> > res;

    // Call the AdminList method
    for (auto &partition: partitions) {
        for (auto &partition_node: partition.nodes) {
            ClientContext context;
            ListRequest req;
            ListResponse resp;
            Status status = partition_node.stub->AdminList(&context, req, &resp);
            // Check if the call was successful
            if (status.ok()) {
                // Iterate through the keys in the ListResponse object
                for (const auto &key_pair: resp.keys()) {
                    // Access the row and column values
                    const std::string &row = key_pair.row();
                    const std::string &col = key_pair.col();
                    res.emplace_back(row, col);
                    // Display the row and column values
                    std::cout << "Row: " << row << ", Col: " << col << std::endl;
                }
                break;
            } else {
                // Handle the error
                std::cerr << "Error: " << status.error_message() << std::endl;
            }

        }
    }

    return res;


}

