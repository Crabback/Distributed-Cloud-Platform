#include "admin_console.h"
#include "src/kv/master_client.h"
#include <iostream>
#include <vector>
#include <utility>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

using namespace std;

//AdminConsole::AdminConsole(vector<MasterPartitionView>& partitions) : partitions(move(partitions)) {}

// get_frontend_nodes: return a vector of strings representing the addresses of all frontend nodes
vector<string> AdminConsole::get_frontend_nodes() {
    vector<string> frontend_servers = {"127.0.0.1:8080", "127.0.0.1:8081", "127.0.0.1:8082"};
    return frontend_servers;
}


//vector<bool> AdminConsole::check_backend_status() {
//	vector<bool > results;
//	KVMasterClient master_client(partitions);
////	results = master_client.do_healthcheck();
//
//    return results;
//}

//// return the status of each frontend server
//vector<bool> AdminConsole::check_frontend_status() {
//    vector<string> frontend_servers= get_frontend_nodes();
//    vector<bool> results;
//
//    return results;
//}

vector<bool> AdminConsole::check_frontend_status() {
    vector<string> frontend_servers = get_frontend_nodes();
    vector<bool> results;

    for (const string& server : frontend_servers) {
        string ip_address = server.substr(0, server.find(":"));
        int port = stoi(server.substr(server.find(":") + 1));
        // Create a TCP socket and connect to the server
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
        server_addr.sin_port = htons(port);
        if (connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == 0) {
            // Send a GET request to the server and read the response
            string request = "GET /status HTTP/1.1\r\n\r\n";
            send(sockfd, request.c_str(), request.length(), 0);
            char buffer[1024];
            int bytes_read = recv(sockfd, buffer, 1024, 0);
            if (bytes_read > 0) {
                string response(buffer, bytes_read);
                // Check if the response starts with "HTTP/1.1 200 OK"
                bool success = (response.find("HTTP/1.1 200 OK") == 0);
                results.push_back(success);
                if (success) {
                	// Log the server IP and the status
					cout << "Frontend server " << server << " status: 200 OK" << endl;
                } else {
                	// Log the server IP and the status
					cout << "Frontend server " << server << " status: false" << endl;
                }
            } else {
                results.push_back(false);
                // Log the server IP and the status
                cout << "Frontend server " << server << " status: false" << endl;
            }
        } else {
            results.push_back(false);
            // Log the server IP and the status
            cout << "Frontend server " << server << " status: false" << endl;
        }
        close(sockfd);
    }

    return results;
}

// * mark a frontend node as unhealthy
// * @param: representing the IP address and port number of the frontend node to be disabled
// * @return: whether disable is successfull
// */
//bool AdminConsole::disable_frontend(const string& node) {
//
//	return true;
//}

bool AdminConsole::disable_frontend(const std::string& node) {
	cerr<<node<<endl;
    string ip_address = node.substr(0, node.find(":"));
    int port = stoi(node.substr(node.find(":") + 1));
    cerr<<ip_address<<endl;
    cerr<<port<<endl;
    // Create a TCP socket and connect to the server
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
    server_addr.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == 0) {
        // Send a POST request to the server's shutdown endpoint
        string request = "GET /shutdown HTTP/1.1\r\n\r\n";

//        unordered_map<string, string> headers;
//		headers.insert(make_pair("Content-Type", "text/plain"));
//		headers.insert(make_pair("Content-Length", to_string(content.size())));
//
//		string request = generate_response("200 OK", headers, content);

        send(sockfd, request.c_str(), request.length(), 0);
        char buffer[1024];
        int bytes_read = recv(sockfd, buffer, 1024, 0);
        if (bytes_read > 0) {
            string response(buffer, bytes_read);
            // Check if the response starts with "HTTP/1.1 200 OK"
            bool success = (response.find("HTTP/1.1 503 Service Unavailable") == 0);
            if (success) {
            	// Log the server IP and the status
				cout << "Frontend server " << node << " disable status: " << "HTTP/1.1 200 OK" << endl;
            } else {
            	// Log the server IP and the status
				cout << "Frontend server " << node << " disable status: false" << endl;
            }
            return success;
        } else {
            // Log the server IP and the status
            cout << "Frontend server " << node << " disable status: false" << endl;
            return false;
        }
    } else {
        // Log the server IP and the status
        cout << "Frontend server " << node << " disable status: false" << endl;
        return false;
    }
    close(sockfd);
}

string AdminConsole::generate_response(const string& status_code, const unordered_map<string, string>& headers, const string& content) {
    ostringstream response;
    response << "HTTP/1.1 " << status_code << "\r\n";
    unordered_map<string, string> headers_copy = headers; // Make a copy of headers to modify
    for (const auto& header : headers_copy) {
        response << header.first << ": " << header.second << "\r\n";
    }
    response << "\r\n" << content;
    return response.str();
}

//bool AdminConsole::disable_backend(const std::string& node) {
//	KVMasterClient client(partitions);
//	return client.adminshutdown(node);
//}
//
//bool AdminConsole::restart_backend(const std::string& node) {
//	KVMasterClient client(partitions);
//	return client.adminstartup(node);
//}


/**
 * restart a frontedn node
 * @param: representing the IP address and port number of the node to be restart
 * @return: whether restart is successfull
 */
bool AdminConsole::restart_frontend(const string& node) {
    string ip_address = node.substr(0, node.find(":"));
    int port = stoi(node.substr(node.find(":") + 1));
    // Create a TCP socket and connect to the server
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
    server_addr.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == 0) {
        // Send a POST request to the server's shutdown endpoint
        string request = "GET /restart HTTP/1.1\r\n\r\n";
        send(sockfd, request.c_str(), request.length(), 0);
        char buffer[1024];
        int bytes_read = recv(sockfd, buffer, 1024, 0);
        if (bytes_read > 0) {
            string response(buffer, bytes_read);
            // Check if the response starts with "HTTP/1.1 200 OK"
            bool success = (response.find("HTTP/1.1 200 OK") == 0);
            if (success) {
				// Log the server IP and the status
            	cout << "Frontend server " << node << " restart status: " << "HTTP/1.1 200 OK" << endl;
			} else {
				// Log the server IP and the status
				cout << "Frontend server " << node << " restart status: false" << endl;
			}
            return success;
        } else {
            // Log the server IP and the status
            cout << "Frontend server " << node << " restart status: false" << endl;
            return false;
        }
    } else {
        // Log the server IP and the status
        cout << "Frontend server " << node << " restart status: false" << endl;
        return false;
    }
    close(sockfd);
}
