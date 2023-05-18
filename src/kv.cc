#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <csignal>
#include <vector>
#include <thread>
#include <fcntl.h>
#include <unistd.h>

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <fmt/core.h>
#include "src/protos/kv.grpc.pb.h"
#include "src/protos/kvmaster.grpc.pb.h"
#include "src/common/utils.h"
#include "src/kv/exceptions.h"

using namespace grpc;
using namespace std;

#define PRIM_UNKNOWN -2
#define PRIM_ME -1

// global server ref
unique_ptr<Server> server;

class KVStoreImpl final : public KVStore::Service {
    string addr, log_path, db_path;
    unsigned partition_idx;

    unique_ptr<KVMaster::Stub> master;
    vector<unique_ptr<KVStore::Stub>> peers;
    const vector<string> peer_addrs;
    int primary_idx;  // indexes peers and peer_addrs

    mutex lock_lock;  // creating a row lock needs to be atomic!
    map<string, mutex> row_locks;
    Tablet tablet;

    int logfd;
    mutex log_lock;
    thread checkpoint_thread;
    bool running = true, serving = true;

    /**
     * Return the mutex for the given row
     */
    mutex &lock_for(const string &row) {
        const lock_guard<mutex> lock(lock_lock);
        return row_locks[row];  // creates a new mutex if the one for the row does not exist
    }

    void load_tablet() {
        ifstream f;
        string line;
        f.open(db_path, ios::in | ios::binary);
        if (!f.is_open()) {
            logf("[WARN] could not open tablet file (%s)\n", strerror(errno));
            return;
        }
        int offset;
        while (getline(f, line)) {
            string op = get_token(line, 0, &offset);
            if (op == "DATA") {
                int rowbytes = stoi(get_token(line, offset + 1, &offset));
                int colbytes = stoi(get_token(line, offset + 1, &offset));
                int databytes = stoi(get_token(line, offset + 1, &offset));
                // row
                string rowk(rowbytes, 0);
                f.read(rowk.data(), rowbytes);
                f.seekg(1, ios_base::cur);
                // col
                string colk(colbytes, 0);
                f.read(colk.data(), colbytes);
                f.seekg(1, ios_base::cur);
                // data
                string data(databytes, 0);
                f.read(data.data(), databytes);
                f.seekg(1, ios_base::cur);
                // op
                do_put(rowk, colk, data);
            } else {
                logf("[ERR] invalid tablet entry: op=%s\n", op.c_str());
                exit(EXIT_FAILURE);
            }
        }
    }

    int save_tablet() {
        const lock_guard<mutex> lock(log_lock);
        // open tablet file
        int fd = open(db_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            logf("[ERR] failed to open tablet file: %s\n", strerror(errno));
            return -1;
        }

        // write each entry
        for (const auto &[rowk, row]: tablet.rows()) {
            for (const auto &[colk, data]: row.cols()) {
                // DATA <SP> rowbytes <SP> colbytes <SP> databytes <LF> row <LF> col <LF>
                string op = fmt::format("DATA {0} {1} {2}\n{3}\n{4}\n",
                                        rowk.size(), colk.size(), data.size(), rowk, colk);
                size_t written = write(fd, op.c_str(), op.size());
                if (written != op.size()) {
                    logf("[ERR] failed to write to tablet: %s\n", strerror(errno));
                    return -1;
                }
                // (*databytes* bytes of data follow) <LF>
                size_t written2 = write(fd, data.c_str(), data.size());
                if (written2 != data.size()) {
                    logf("[ERR] failed to write to tablet: %s\n", strerror(errno));
                    return -1;
                }
                // newline
                if (write(fd, "\n", 1) != 1) {
                    logf("[ERR] failed to write to tablet: %s\n", strerror(errno));
                    return -1;
                }
            }
        }

        // if the tablet is successfully saved, clear the log
        if (ftruncate(logfd, 0) == -1) {
            logf("[ERR] failed to trunc logfile: %s\n", strerror(errno));
        }
        logf("[INFO] Tablet checkpointed\n");
        return 0;
    }

    void sync_tablet_from_peer(const unique_ptr<KVStore::Stub> &peer) {
        const lock_guard<mutex> lock(log_lock);
        ClientContext ctx;
        TabletSyncRequest req;
        unique_ptr<ClientReader<KeyValue>> reader(peer->TabletSync(&ctx, req));

        // stream kvs
        KeyValue kv;
        while (reader->Read(&kv)) {
            do_put(kv.row(), kv.col(), kv.value());
            if (DEBUG)
                logf("[DBUG] tablet sync: %s, %s\n", kv.row().c_str(), kv.col().c_str());
        }

        Status status = reader->Finish();
        if (!status.ok()) {
            logf("[WARN] failed to sync tablet from peer: %s\n",
                 grpc_status_to_string(status.error_code()).c_str());
        } else {
            logf("[INFO] synced tablet from peer\n");
        }
    }

    int get_peer_idx(const string &p_addr) {
        auto it = find(peer_addrs.begin(), peer_addrs.end(), p_addr);
        if (it == peer_addrs.end()) {
            return -1;
        }
        return distance(peer_addrs.begin(), it);
    }

    void update_primary_idx(const string &p_addr) {
        if (p_addr == addr) {
            primary_idx = PRIM_ME;
            logf("[INFO] primary index is now PRIM_ME\n");
            return;
        }
        int idx = get_peer_idx(p_addr);
        if (idx == -1) {
            primary_idx = PRIM_UNKNOWN;
            logf("[INFO] primary index is now PRIM_UNKNOWN\n");
            return;
        }
        primary_idx = idx;
        logf("[INFO] primary index is now %d (%s)\n", primary_idx, peer_addrs[primary_idx].c_str());
    }

    /**
     * Apply all the operations in the log file to the tablet.
     * @return the number of operations successfully applied; -1 on error
     */
    int apply_log() {
        const lock_guard<mutex> lock(log_lock);
        ifstream f;
        string line;
        f.open(log_path, ios::in | ios::binary);
        if (!f.is_open()) {
            logf("[WARN] could not open log file to read (%s)\n", strerror(errno));
            return -1;
        }
        int ops = 0, offset;
        while (getline(f, line)) {
            // look for PUT or DELETE
            string op = get_token(line, 0, &offset);
            if (op == "DELETE") {
                int rowbytes = stoi(get_token(line, offset + 1, &offset));
                int colbytes = stoi(get_token(line, offset + 1, &offset));
                // row
                string rowk(rowbytes, 0);
                f.read(rowk.data(), rowbytes);
                f.seekg(1, ios_base::cur);
                // col
                string colk(colbytes, 0);
                f.read(colk.data(), colbytes);
                f.seekg(1, ios_base::cur);
                // op
                logf("[INFO] applying log operation: DELETE %s %s\n", rowk.c_str(), colk.c_str());
                do_delete(rowk, colk);
                ops++;
                // seek to next line
                f.seekg(1, ios_base::cur);
            } else if (op == "PUT") {
                int rowbytes = stoi(get_token(line, offset + 1, &offset));
                int colbytes = stoi(get_token(line, offset + 1, &offset));
                int databytes = stoi(get_token(line, offset + 1, &offset));
                // row
                string rowk(rowbytes, 0);
                f.read(rowk.data(), rowbytes);
                f.seekg(1, ios_base::cur);
                // col
                string colk(colbytes, 0);
                f.read(colk.data(), colbytes);
                f.seekg(1, ios_base::cur);
                // data
                string data(databytes, 0);
                f.read(data.data(), databytes);
                f.seekg(1, ios_base::cur);
                // op
                logf("[INFO] applying log operation: PUT %s %s %dB\n", rowk.c_str(), colk.c_str(), data.size());
                do_put(rowk, colk, data);
                ops++;
            } else {
                logf("[WARN] invalid log entry: op=%s\n", op.c_str());
            }
        }
        return ops;
    }

    /**
     * Ask the master node who the primary of this node's partition is, and update the local primary view.
     * Returns empty string if the primary is unknown (master down, node is first in partition up).
     */
    GetPrimaryResponse refresh_primary() {
        ClientContext ctx;
        GetPrimaryRequest req;
        GetPrimaryResponse resp;
        req.set_partition_idx(partition_idx);
        Status status = master->GetPartitionPrimary(&ctx, req, &resp);
        if (!status.ok()) {
            logf("[WARN] unable to connect to master node to learn about primary: %s\n",
                 grpc_status_to_string(status.error_code()).c_str());
            throw KVError(status);
        }
        update_primary_idx(resp.addr());
        return resp;
    }

    /**
     * Ensure the node has correctly joined the partition. Returns 1 if we know the primary, 0 otherwise.
     */
    int ensure_partition_joined() {
        if (primary_idx == PRIM_UNKNOWN)
            refresh_primary();
        return primary_idx != PRIM_UNKNOWN;
    }

    void checkpoint_task() {
        while (true) {
            int i = 0;
            save_tablet();
            // sleep for 1 minute, cancel task if shutting down
            while (i < 60) {
                sleep(1);
                if (!running) return;
                i++;
            }
        }
    }

    // ==== KV impls ====
    int log_put(const string &rowk, const string &colk, const string &data) {
        const lock_guard<mutex> lock(log_lock);
        // PUT <SP> rowbytes <SP> colbytes <SP> databytes <LF> row <LF> col <LF>
        string op = fmt::format("PUT {0} {1} {2}\n{3}\n{4}\n", rowk.size(), colk.size(), data.size(), rowk, colk);
        size_t written = write(logfd, op.c_str(), op.size());
        if (written != op.size()) {
            logf("[ERR] failed to write to logfile: %s\n", strerror(errno));
            return -1;
        }
        // (*databytes* bytes of data follow) <LF>
        size_t written2 = write(logfd, data.c_str(), data.size());
        if (written2 != data.size()) {
            logf("[ERR] failed to write to logfile: %s\n", strerror(errno));
            return -1;
        }
        // newline
        if (write(logfd, "\n", 1) != 1) {
            logf("[ERR] failed to write to logfile: %s\n", strerror(errno));
            return -1;
        }
        return written + written2 + 1;
    }

    unsigned do_put(const string &rowk, const string &colk, const string &data) {
        // commit change to mem
        auto row = (*tablet.mutable_rows())[rowk];
        (*row.mutable_cols())[colk] = data;
        (*tablet.mutable_rows())[rowk] = row;
        return data.size();
    }

    int log_delete(const string &rowk, const string &colk) {
        const lock_guard<mutex> lock(log_lock);
        // DELETE <SP> rowbytes <SP> colbytes <LF> row <LF> col <LF>
        string op = fmt::format("DELETE {0} {1}\n{2}\n{3}\n", rowk.size(), colk.size(), rowk, colk);
        size_t written = write(logfd, op.c_str(), op.size());
        if (written != op.size()) {
            logf("[ERR] failed to write to logfile: %s\n", strerror(errno));
            return -1;
        }
        return written;
    }

    unsigned do_delete(const string &rowk, const string &colk) {
        // find row
        if (!tablet.rows().contains(rowk))
            return 0;
        auto row = tablet.rows().at(rowk);
        if (!row.cols().contains(colk))
            return 0;

        // commit change to mem
        row.mutable_cols()->erase(colk);
        (*tablet.mutable_rows())[rowk] = row;
        return 1;
    }

    // ==== primary utils ====
    static void _multicast_put_inner(KVStore::Stub *node, const string &rowk, const string &colk,
                                     const string &data, Status *status, PutResponse *resp) {
        ClientContext ctx;
        PutRequest req;
        PutResponse resp2;
        req.set_row(rowk);
        req.set_col(colk);
        req.set_value(data);
        *status = node->PPut(&ctx, req, &resp2);
        resp->CopyFrom(resp2);  // grpc does not like passing the resp directly
    }

    void multicast_put(const string &rowk, const string &colk, const string &data) {
        vector<thread> threads;
        vector<Status> statuses;
        vector<PutResponse> responses;
        for (auto &node: peers) {
            auto status = statuses.emplace_back();
            auto resp = responses.emplace_back();
            threads.emplace_back(KVStoreImpl::_multicast_put_inner, node.get(), rowk, colk, data, &status, &resp);
        }

        for (int i = 0; i < threads.size(); i++) {
            auto &t = threads[i];
            auto &s = statuses[i];
            t.join();
            if (!s.ok()) {
                logf("[ERR] multicast_put[%d] fail: %s %s\n", i,
                     grpc_status_to_string(s.error_code()).c_str(), s.error_message().c_str());
            }
        }
    }

    static void _multicast_delete_inner(KVStore::Stub *node, const string &rowk, const string &colk,
                                        Status *status, DeleteResponse *resp) {
        ClientContext ctx;
        DeleteRequest req;
        DeleteResponse resp2;
        req.set_row(rowk);
        req.set_col(colk);
        *status = node->PDelete(&ctx, req, &resp2);
        resp->CopyFrom(resp2);  // grpc does not like passing the resp directly
    }

    void multicast_delete(const string &rowk, const string &colk) {
        vector<thread> threads;
        vector<Status> statuses;
        vector<DeleteResponse> responses;
        for (auto &node: peers) {
            auto status = statuses.emplace_back();
            auto resp = responses.emplace_back();
            threads.emplace_back(KVStoreImpl::_multicast_delete_inner, node.get(), rowk, colk, &status, &resp);
        }

        for (int i = 0; i < threads.size(); i++) {
            auto &t = threads[i];
            auto &s = statuses[i];
            t.join();
            if (!s.ok()) {
                logf("[ERR] multicast_delete[%d] fail: %s %s\n", i,
                     grpc_status_to_string(s.error_code()).c_str(), s.error_message().c_str());
            }
        }
    }

public:
    KVStoreImpl(const string &addr, const string &log_path, const string &db_path,
                const string &master_addr, const vector<string> &partition_peer_addrs,
                int partition_idx)
            : addr(addr), log_path(log_path), db_path(db_path),
              partition_idx(partition_idx), peer_addrs(partition_peer_addrs),
              primary_idx(PRIM_UNKNOWN) {
        // init master stub
        auto mchannel = CreateChannel(master_addr, InsecureChannelCredentials());
        master = KVMaster::NewStub(mchannel);
        // init peer stubs
        for (const auto &peer_addr: partition_peer_addrs) {
            auto channel = CreateChannel(peer_addr, InsecureChannelCredentials());
            peers.push_back(KVStore::NewStub(channel));
        }
    }

    // ==== KV RPC impls ====
    Status Put(ServerContext *context, const PutRequest *req, PutResponse *resp) override {
        if (!serving)
            return Status(StatusCode::UNAVAILABLE, "Node disabled by admin");

        auto &rowk = req->row();
        auto &colk = req->col();
        auto &data = req->value();
        // row, col, value all required
        if (rowk.empty() || colk.empty()) {
            return Status(StatusCode::INVALID_ARGUMENT, "row and col are required");
        }

        // ensure we know who the primary is
        if (!ensure_partition_joined())
            return Status(StatusCode::UNAVAILABLE, "node not connected to partition");
        // apply or forward to primary
        if (primary_idx == PRIM_ME) {
            // lock row
            const lock_guard<mutex> lock(lock_for(rowk));
            // multicast to peers
            multicast_put(rowk, colk, data);
            // apply locally
            log_put(rowk, colk, data);
            unsigned bytes_written = do_put(rowk, colk, data);
            resp->set_bytes_written(bytes_written);
            return Status::OK;
        }
        ClientContext ctx;
        auto &primary = peers[primary_idx];
        return primary->Put(&ctx, *req, resp);
    }

    Status Get(ServerContext *context, const GetRequest *req, GetResponse *resp) override {
        if (!serving)
            return Status(StatusCode::UNAVAILABLE, "Node disabled by admin");

        const auto &rowk = req->row();
        const auto &colk = req->col();
        {
            // lock row
            const lock_guard<mutex> lock(lock_for(rowk));

            if (!tablet.rows().contains(rowk)) {
                return Status(StatusCode::NOT_FOUND, "row not found");
            }
            auto row = tablet.rows().at(rowk);
            if (!row.cols().contains(colk)) {
                return Status(StatusCode::NOT_FOUND, "col not found");
            }
            auto value = row.cols().at(colk);
            resp->mutable_data()->set_row(rowk);
            resp->mutable_data()->set_col(colk);
            resp->mutable_data()->set_value(value);
        }
        return Status::OK;
    }

    Status CPut(ServerContext *context, const CPutRequest *req, CPutResponse *resp) override {
        if (!serving)
            return Status(StatusCode::UNAVAILABLE, "Node disabled by admin");

        auto &rowk = req->row();
        auto &colk = req->col();
        // row, col, value all required
        if (rowk.empty() || colk.empty()) {
            return Status(StatusCode::INVALID_ARGUMENT, "row and col are required");
        }

        // ensure we know who the primary is
        if (!ensure_partition_joined())
            return Status(StatusCode::UNAVAILABLE, "node not connected to partition");
        // apply or forward to primary
        if (primary_idx == PRIM_ME) {
            auto &v1 = req->v1();
            auto &v2 = req->v2();
            // lock row
            const lock_guard<mutex> lock(lock_for(rowk));

            // check condition
            if (!(tablet.rows().contains(rowk) || v1.empty()))
                return Status(StatusCode::NOT_FOUND, "row not found");
            auto row = (*tablet.mutable_rows())[rowk];
            if (!(row.cols().contains(colk) || v1.empty()))
                return Status(StatusCode::NOT_FOUND, "col not found");

            // options to allow write:
            // row[col] does not exist and v1 is empty
            // row[col] exists and equals v1
            if (!(
                    (!row.cols().contains(colk) && v1.empty())
                    || (row.cols().contains(colk) && row.cols().at(colk) == v1)
            ))
                return Status(StatusCode::FAILED_PRECONDITION, "value does not match");

            // multicast to peers
            multicast_put(rowk, colk, v2);
            // apply locally
            log_put(rowk, colk, v2);
            unsigned bytes_written = do_put(rowk, colk, v2);
            resp->set_bytes_written(bytes_written);
            return Status::OK;
        }
        ClientContext ctx;
        auto &primary = peers[primary_idx];
        return primary->CPut(&ctx, *req, resp);
    }

    Status Delete(ServerContext *context, const DeleteRequest *req, DeleteResponse *resp) override {
        if (!serving)
            return Status(StatusCode::UNAVAILABLE, "Node disabled by admin");

        auto &rowk = req->row();
        auto &colk = req->col();

        // ensure we know who the primary is
        if (!ensure_partition_joined())
            return Status(StatusCode::UNAVAILABLE, "node not connected to partition");
        // apply or forward to primary
        if (primary_idx == PRIM_ME) {
            // lock row
            const lock_guard<mutex> lock(lock_for(rowk));
            // multicast to peers
            multicast_delete(rowk, colk);
            // apply locally
            log_delete(rowk, colk);
            unsigned keys_deleted = do_delete(rowk, colk);
            resp->set_keys_deleted(keys_deleted);
            return Status::OK;
        }
        ClientContext ctx;
        auto &primary = peers[primary_idx];
        return primary->Delete(&ctx, *req, resp);
    }

    // ==== primary -> replica impls ====
    Status PPut(ServerContext *context, const PutRequest *req, PutResponse *resp) override {
        if (!serving)
            return Status(StatusCode::UNAVAILABLE, "Node disabled by admin");

        // lock row
        const lock_guard<mutex> lock(lock_for(req->row()));
        // apply locally
        log_put(req->row(), req->col(), req->value());
        unsigned bytes_written = do_put(req->row(), req->col(), req->value());
        resp->set_bytes_written(bytes_written);
        return Status::OK;
    }


    Status PDelete(ServerContext *context, const DeleteRequest *req, DeleteResponse *resp) override {
        if (!serving)
            return Status(StatusCode::UNAVAILABLE, "Node disabled by admin");

        // lock row
        const lock_guard<mutex> lock(lock_for(req->row()));
        // apply locally
        log_delete(req->row(), req->col());
        unsigned keys_deleted = do_delete(req->row(), req->col());
        resp->set_keys_deleted(keys_deleted);
        return Status::OK;
    }

    // ==== master -> kv impls ====
    Status HealthCheck(ServerContext *context, const HealthRequest *req, HealthResponse *resp) override {
        if (!serving)
            return Status(StatusCode::UNAVAILABLE, "Node disabled by admin");
        // health check simply returns OK to say the server is alive
        return Status::OK;
    }

    Status PrimaryUpdate(ServerContext *context, const PrimaryRequest *req, PrimaryResponse *resp) override {
        if (!serving)
            return Status(StatusCode::UNAVAILABLE, "Node disabled by admin");
        update_primary_idx(req->addr());
        return Status::OK;
    }

    // ==== kv -> kv impls ====
    Status TabletSync(ServerContext *context, const TabletSyncRequest *req, ServerWriter<KeyValue> *writer) override {
        if (!serving)
            return Status(StatusCode::UNAVAILABLE, "Node disabled by admin");
        const lock_guard<mutex> lock(log_lock);
        // stream all the KVs in the local tablet
        for (const auto &[rowk, row]: tablet.rows()) {
            for (const auto &[colk, data]: row.cols()) {
                auto kv = KeyValue();
                kv.set_row(rowk);
                kv.set_col(colk);
                kv.set_value(data);
                writer->Write(kv);
            }
        }
        return Status::OK;
    }

    // ==== admin -> kv impls ====
    Status AdminShutdown(ServerContext *context, const ShutdownRequest *req, ShutdownResponse *resp) override {
        serving = false;
        return Status::OK;
    }

    Status AdminStartup(ServerContext *context, const StartupRequest *req, StartupResponse *resp) override {
        serving = true;
        return Status::OK;
    }

    Status AdminList(ServerContext *context, const ListRequest *req, ListResponse *resp) override {
        vector<pair<string, vector<string>>> rows;
        for (const auto &[rowk, row]: tablet.rows()) {
            auto &[_, row_cols] = rows.emplace_back(rowk, vector<string>());
            for (const auto &[colk, data]: row.cols()) {
                row_cols.push_back(colk);
            }
            // sort cols
            sort(row_cols.begin(), row_cols.end());
        }
        // sort rows
        sort(rows.begin(), rows.end(),
             [](pair<string, vector<string>> &a, pair<string, vector<string>> &b) ->
                     int { return a.first < b.first; });
        // write to out
        for (const auto &[rowk, cols]: rows) {
            for (const auto &colk: cols) {
                auto pair = resp->add_keys();
                pair->set_row(rowk);
                pair->set_col(colk);
            }
        }
        return Status::OK;
    }

    // ==== helpers ====
    void init() {
        // read the data from disk into memory
        load_tablet();
        // read the log and update the working data
        apply_log();
        // init log file
        logfd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (logfd == -1) {
            logf("[ERR] failed to open log file: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        // ask the master who the primary of the partition is
        try {
            auto resp = refresh_primary();
            // get tablet from peer if any are alive
            int peer_idx = -1;
            for (auto &node: resp.nodes()) {
                if (node.alive() && node.address() != addr) {
                    peer_idx = get_peer_idx(node.address());
                    break;
                }
            }
            if (peer_idx != -1) {
                sync_tablet_from_peer(peers[peer_idx]);
            } else {
                logf("[WARN] no peers are up for tablet sync\n");
            }
        } catch (KVError &) {

        }
        // launch checkpoint thread to save tablet every minute
        checkpoint_thread = thread(&KVStoreImpl::checkpoint_task, this);
    }

    void close() {
        running = false;
        checkpoint_thread.join();
        save_tablet();
        ::close(logfd);
    }
};


// main entrypoints
void do_shutdown() {
    logf("[INFO] Shutting down...\n");
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
    // ./kv <db file path> <log file path> <path to config> <partition no, 1-indexed> <node index, 0-indexed>
    // ex. bazel-bin/src/kv data/db1.bin data/log1.bin kvconfig_min.txt 1 0
    if (argc != 6) {
        logf("[ERR] Usage: %s <db_file> <log_file> <config_file> <partition_no> <node_no>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    string db_path = argv[1], log_path = argv[2], config_path = argv[3];
    int partition_line_idx = stoi(argv[4]), node_idx = stoi(argv[5]);

    if (partition_line_idx < 1) {
        logf("[ERR] Partition index must be at least 1\n");
        exit(EXIT_FAILURE);
    }

    // read and parse config file
    vector<string> partition_peers;
    string master_addr;
    // open the file (or error if we can't)
    string line;
    ifstream f;
    f.open(config_path, ios::in);
    if (!f.is_open()) {
        logf("[ERR] Could not open file: %s\n", config_path.c_str());
        exit(EXIT_FAILURE);
    }
    // 1st line is the master address
    if (!getline(f, master_addr)) {
        logf("[ERR] Config file should have KV master addr on first line\n");
        exit(EXIT_FAILURE);
    }
    // ith line is the partition index, skip lines
    for (int i = 1; i < partition_line_idx; ++i)
        getline(f, line);
    // read the partition line
    if (!getline(f, line)) {
        logf("[ERR] Given partition config is empty\n");
        exit(EXIT_FAILURE);
    }
    int i = 0, j = 0;
    while (j != string::npos) {
        j = line.find(',', i);
        partition_peers.push_back(line.substr(i, j == string::npos ? j : j - i));
        i = j + 1;
    }
    // and close the file
    f.close();

    // get the local server and remove it from the other server list
    if (!(node_idx >= 0 && node_idx < partition_peers.size())) {
        logf("[ERR] Provided node index out of bounds: %d\n", node_idx);
        exit(EXIT_FAILURE);
    }
    string server_address = partition_peers[node_idx];
    partition_peers.erase(partition_peers.begin() + node_idx);

    // register signal handler
    signal(SIGINT, handle_signal);

    // serve KV store
    KVStoreImpl service = KVStoreImpl(server_address, log_path, db_path, master_addr, partition_peers,
                                      partition_line_idx - 1);
    service.init();

    ServerBuilder builder;
    builder.AddListeningPort(server_address, InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    logf("[INFO] Server listening on %s\n", server_address.c_str());
    server->Wait();

    // after shutdown
    service.close();
}