/**
 * This test binary connects to the KV master hosted at 127.0.0.1:5000 and increases the (test, foo) key by one.
 * Useful for testing the master node, gets, and sets.
 */

#include <iostream>
#include "src/kv/client.h"

using namespace std;

int main(int argc, char *argv[]) {
    KVClient client("127.0.0.1:5000");

    while (true) {
        // get foo
        int foo;
        try {
            auto foo_val = client.get("test", "foo");
            foo = stoi(foo_val);
            cout << "GET foo=" << foo << endl;
        } catch (KVNotFound &) {
            foo = 0;
            cout << "GET foo=undefined" << endl;
        } catch (KVError &err) {
            cout << "GET RPC failed: " << err.what() << endl;
        }

        // incr foo
        foo++;
        try {
            client.put("test", "foo", to_string(foo));
            cout << "PUT foo=" << foo << endl;
        } catch (KVError &err) {
            cout << "PUT RPC failed: " << err.what() << endl;
        }

        sleep(1);
    }
    return 0;
}
