/**
 * This test binary runs a pseudo-shell to interact with the KV servers.
 * Commands:
 *  PUT row col data
 *  GET row col
 *  CPUT row col v1 v2
 *  DELETE row col
 */

#include <iostream>
#include "src/kv/client.h"

using namespace std;

int main(int argc, char *argv[]) {
    KVClient client("127.0.0.1:5000");

    while (true) {
        fprintf(stderr, "kvshell> ");
        // get command
        string line;
        getline(cin, line);

        int offset;
        string cmd = get_token(line, 0, &offset);

        if (eq_case(cmd, "PUT")) {
            string row = get_token(line, offset + 1, &offset);
            string col = get_token(line, offset + 1, &offset);
            string data = line.substr(offset + 1);
            try {
                int written = client.put(row, col, data);
                cout << written << endl;
            } catch (KVError &err) {
                cout << "PUT RPC failed: " << err.what() << endl;
            }
        } else if (eq_case(cmd, "GET")) {
            string row = get_token(line, offset + 1, &offset);
            string col = get_token(line, offset + 1, &offset);
            try {
                string data = client.get(row, col);
                cout << data << endl;
            } catch (KVError &err) {
                cout << "GET RPC failed: " << err.what() << endl;
            }
        } else if (eq_case(cmd, "CPUT")) {
            string row = get_token(line, offset + 1, &offset);
            string col = get_token(line, offset + 1, &offset);
            string v1 = get_token(line, offset + 1, &offset);
            string v2 = line.substr(offset + 1);
            try {
                int written = client.cput(row, col, v1, v2);
                cout << written << endl;
            } catch (KVError &err) {
                cout << "CPUT RPC failed: " << err.what() << endl;
            }
        } else if (eq_case(cmd, "DELETE")) {
            string row = get_token(line, offset + 1, &offset);
            string col = get_token(line, offset + 1, &offset);
            try {
                int deleted = client.del(row, col);
                cout << deleted << endl;
            } catch (KVError &err) {
                cout << "DELETE RPC failed: " << err.what() << endl;
            }
        } else {
            logf("unknown command: %s\n", cmd.c_str());
        }
    }
}
