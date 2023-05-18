#include <arpa/inet.h>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "request_handler.h"

using namespace std;

// unique_ptr<bool> serving(true);
bool serving = true;

KVClient *kvClient;
SessionManager *session_manager;

class FrontendServer {
  public:
    FrontendServer(int port);
    ~FrontendServer();
    void run();

  private:
    struct ThreadArgs {
        ThreadArgs(FrontendServer *server, int client_socket) : server(server), client_socket(client_socket) {}
        FrontendServer *server;
        int client_socket;
    };
    static void *handle_client_wrapper(void *args);
    void handle_client(int client_socket);
    int server_socket;
    int port;
    mutex client_mutex;
};

FrontendServer::FrontendServer(int port) : port(port) {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (::bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_socket, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}

FrontendServer::~FrontendServer() { close(server_socket); }

void *FrontendServer::handle_client_wrapper(void *args) {
    ThreadArgs *thread_args = static_cast<ThreadArgs *>(args);
    // cerr << "debug3\n";
    try {
        thread_args->server->handle_client(thread_args->client_socket);
    } catch (exception &e) {
        logf("[ERR] %s\n", e.what());
    }
    // cerr << "debug4\n";
    delete thread_args;
    return nullptr;
}

void FrontendServer::handle_client(int client_socket) {
    lock_guard<mutex> lock(client_mutex);

    // Read request from the client
    // char buffer[16777216] = {0};
    char *buffer = new char[16777216];
    memset(buffer, 0, 16777216);
    // char buffer[4096] = {0};
    ssize_t bytes_read = recv(client_socket, buffer, 16777216 - 1, 0);
    if (bytes_read <= 0) {
        perror("recv");
        delete[] buffer;
        close(client_socket);
        return;
    }
    string raw_request(buffer, bytes_read);

    // Parse the request using the RequestHandler class
    RequestHandler request_handler(session_manager, kvClient, serving);
    string response = request_handler.handle_request(raw_request);
    // cout << response.data() << endl;
    // handle shutdown if (response == )

    // Send the response back to the client
    send(client_socket, response.data(), response.size(), 0);
    delete[] buffer;
    // Close the client socket
    close(client_socket);
}

void FrontendServer::run() {
    kvClient = new KVClient("127.0.0.1:5000");
    session_manager = new SessionManager();

    while (true) {
        int client_socket = accept(server_socket, NULL, NULL);
        ThreadArgs *args = new ThreadArgs(this, client_socket);
        pthread_t client_thread;
        pthread_create(&client_thread, nullptr, &FrontendServer::handle_client_wrapper, args);
        pthread_detach(client_thread);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << "SHould input port number here.";
        return 0;
    }
    //	cerr<<"debug20\n";
    FrontendServer server(atoi(argv[1]));
    //    cerr<<"debug60\n";
    server.run();
    return 0;
}
