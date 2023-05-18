/**
 * This test binary connects to the KV master hosted at 127.0.0.1:5000 and post three usernames and passwords.
 * Useful for testing the master node, gets, and sets.
 */

#include <iostream>
#include "src/kv/client.h"

using namespace std;

vector<string> username{"admin1", "admin2", "admin3"};
vector<string> password{"123", "123456", "password"};

int main(int argc, char *argv[]) {
    KVClient client("127.0.0.1:5000");
    int i = 0;

    while (i < 3) {
        try {
            client.put(username[i], "password", password[i]);
            cout << "PUT " << username[i] << ": " << password[i] << endl;
        } catch (KVError &err) {
            cout << "PUT Password failed: " << err.what() << endl;
        }

        i++;

        sleep(1);
    }
    return 0;
}
