#include "client.h"

// ==== Partition ====
Partition::Partition(vector<Node> &n) : nodes(std::move(n)) {
    ChannelArguments args;
    args.SetLoadBalancingPolicyName("round_robin");
    string node_addrs = "ipv4:";
    for (int i = 0; i < nodes.size(); i++) {
        node_addrs += nodes[i].addr;
        if (i < nodes.size() - 1) node_addrs += ",";
    }
    auto channel = CreateCustomChannel(node_addrs, InsecureChannelCredentials(), args);
    stub = KVStore::NewStub(channel);
    if (DEBUG)
        logf("[DBG] partition initialized with nodes: %s\n", node_addrs.c_str());
}

// ==== KVClient ====
// ==== internals ====
Partition &KVClient::row_partition(const string &row) {
    uint8_t hash[MD5_DIGEST_LENGTH];
    MD5((const uint8_t *) row.c_str(), row.size(), hash);
    int partition_idx = hash[0] % partitions.size();
    return partitions[partition_idx];
}

// --- kvmaster ---
/**
 * Ask the master node for the current view of the partitions.
 * @throw KVError if the master is unreachable or an other error occurs
 */
void KVClient::refresh_partitions() {
    ClientContext ctx;
    NodeViewRequest req;
    NodeView resp;
    Status status = master_stub->GetNodeView(&ctx, req, &resp);
    if (!status.ok())
        throw KVError(status);
    if (DEBUG)
        logf("[DBG] GetNodeView: %s\n", resp.DebugString().c_str());
    // create a local vector, then swap it with the member in order to do the update atomically
    vector<Partition> parts;
    for (auto &part: resp.partition()) {
        vector<Node> nodes;
        for (auto &node: part.node()) {
            nodes.emplace_back(node.address(), node.alive());
        }
        parts.emplace_back(nodes);
    }
    partitions = std::move(parts);
}

KVClient::KVClient(const string &master_addr) {
    auto channel = CreateChannel(master_addr, InsecureChannelCredentials());
    master_stub = KVMaster::NewStub(channel);
    // ask the master about the partitions
    refresh_partitions();
}

// ==== KV API ====
/**
 * Write data to the KV store.
 * @param row the row key
 * @param col the column key
 * @param data the data to write; this can be an arbitrary bytestring
 * @throw KVError if some error occurred
 * @return the number of bytes written
 */
int KVClient::put(const string &row, const string &col, const string &data) {
    auto &part = row_partition(row);
    // retry up to 5 times
    Status status;
    for (int i = 0; i < 5; ++i) {
        ClientContext ctx;
        PutRequest req;
        PutResponse resp;
        req.set_row(row);
        req.set_col(col);
        req.set_value(data);

        status = part.stub->Put(&ctx, req, &resp);
        if (!status.ok())
            continue;
        if (DEBUG)
            logf("[DBG] put(%s): %d\n", ctx.peer().c_str(), resp.bytes_written());
        return resp.bytes_written();
    }
    throw KVError(status);
}

/**
 * Get data from the KV store.
 * @param row the row key
 * @param col the column key
 * @throw KVNotFound if the key is not found
 * @throw KVError if some other error occurred
 * @return the data in the key
 */
string KVClient::get(const string &row, const string &col) {
    auto &part = row_partition(row);
    // retry up to 5 times
    Status status;
    for (int i = 0; i < 5; ++i) {
        ClientContext ctx;
        GetRequest req;
        GetResponse resp;
        req.set_row(row);
        req.set_col(col);

        status = part.stub->Get(&ctx, req, &resp);
        if (!status.ok()) {
            if (status.error_code() == StatusCode::NOT_FOUND)
                throw KVNotFound(status);
            continue;
        }
        if (DEBUG)
            logf("[DBG] get(%s): %d bytes\n", ctx.peer().c_str(), resp.data().value().size());
        return resp.data().value();
    }
    throw KVError(status);
}

/**
 * Get data from the KV store, returning *def_val* if not found.
 * @param row the row key
 * @param col the column key
 * @param def_val the value to return if the row or col is not yet set
 * @throw KVError if some other error occurred
 * @return the data in the key
 */
string KVClient::getdefault(const string &row, const string &col, const string &def_val) {
    try {
        return get(row, col);
    } catch (KVNotFound &) {
        return def_val;
    }
}

/**
 * Write data to the KV store.
 * @param row the row key
 * @param col the column key
 * @param v1 the value to compare the existing key to (can be empty string to allow creation if key does not exist)
 * @param v2 the data to write; this can be an arbitrary bytestring
 * @throw KVNotFound if v1 is not empty and the key is not found
 * @throw KVNoMatch if the key is set and does not match v1
 * @throw KVError if some other error occurred
 * @return the number of bytes written
 */
int KVClient::cput(const string &row, const string &col, const string &v1, const string &v2) {
    auto &part = row_partition(row);
    // retry up to 5 times
    Status status;
    for (int i = 0; i < 5; ++i) {
        ClientContext ctx;
        CPutRequest req;
        CPutResponse resp;
        req.set_row(row);
        req.set_col(col);
        req.set_v1(v1);
        req.set_v2(v2);

        status = part.stub->CPut(&ctx, req, &resp);
        if (!status.ok()) {
            if (status.error_code() == StatusCode::NOT_FOUND)
                throw KVNotFound(status);
            if (status.error_code() == StatusCode::FAILED_PRECONDITION)
                throw KVNoMatch(status);
            continue;
        }
        if (DEBUG)
            logf("[DBG] cput(%s): %d bytes\n", ctx.peer().c_str(), resp.bytes_written());
        return resp.bytes_written();
    }
    throw KVError(status);
}

/**
 * Delete a key from the KV store.
 * @param row the row key
 * @param col the column key
 * @throw KVError if some error occurred
 * @return the number of keys deleted (0 if the key was not found)
 */
int KVClient::del(const string &row, const string &col) {
    auto &part = row_partition(row);
    // retry up to 5 times
    Status status;
    for (int i = 0; i < 5; ++i) {
        ClientContext ctx;
        DeleteRequest req;
        DeleteResponse resp;
        req.set_row(row);
        req.set_col(col);

        status = part.stub->Delete(&ctx, req, &resp);
        if (!status.ok())
            continue;
        if (DEBUG)
            logf("[DBG] delete(%s): %d\n", ctx.peer().c_str(), resp.keys_deleted());
        return resp.keys_deleted();
    }
    throw KVError(status);
}
