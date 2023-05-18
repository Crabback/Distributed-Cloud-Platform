#include "admin_console.h"
#include <iostream>
#include <thread>
#include <string>
#include <fstream>
#include <csignal>
#include <utility>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>
#include "nlohmann/json.hpp"
#include "src/kv/client.h"


using json = nlohmann::json;

using namespace std;

KVClient *kvClient;

//string generate_response(const string& status_code, const unordered_map<string, string>& headers, const string& content) {
//    ostringstream response;
//    response << "HTTP/1.1 " << status_code << "\r\n";
//    unordered_map<string, string> headers_copy = headers; // Make a copy of headers to modify
//    headers_copy["Content-Type"] = "text/plain";
//    headers_copy["Content-Length"] = to_string(content.size());
//    for (const auto& header : headers_copy) {
//        response << header.first << ": " << header.second << "\r\n";
//    }
//    response << "\r\n" << content;
//    return response.str();
//}

string generate_response(const string &status_code, const unordered_map<string, string> &headers, const string &content) {
    ostringstream response;
    response << "HTTP/1.1 " << status_code << "\r\n";
    unordered_map<string, string> headers_copy = headers; // Make a copy of headers to modify
    for (const auto &header : headers_copy) {
        response << header.first << ": " << header.second << "\r\n";
    }
    response << "\r\n" << content;
    return response.str();
}

string trim(const string& str) {
    string whitespace = " \t\r\n";
    size_t start = str.find_first_not_of(whitespace);
    size_t end = str.find_last_not_of(whitespace);
    if (start == string::npos || end == string::npos) {
        return "";
    }
    return str.substr(start, end - start + 1);
}

unordered_map<string, string> parse_headers(const string& raw_headers) {
    unordered_map<string, string> headers;
    istringstream ss(raw_headers);
    string line;

    while (getline(ss, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        auto colon_pos = line.find(':');
        if (colon_pos != string::npos) {
            string key = trim(line.substr(0, colon_pos));
            string value = trim(line.substr(colon_pos + 1));
            headers[key] = value;
        }
    }

    return headers;
}

vector<string> split(const string& str, char delimiter) {
    vector<string> tokens;
    istringstream ss(str);
    string token;

    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

string serve_webpage() {
	// Get the current working directory
	char buffer[PATH_MAX];
	getcwd(buffer, sizeof(buffer));
	string working_dir(buffer);
    // Read the contents of the HTML file
    ifstream file(working_dir + "/fe_pages/admin.html");
    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    // Set the response headers
    unordered_map<string, string> headers;
    headers["Content-Type"] = "text/html";
    headers["Content-Length"] = to_string(content.length());

    // Generate the response
    string response = generate_response("200 OK", headers, content);

    return response;
}

string generate_redirect(const string& location) {
    string content = "";
    unordered_map<string, string> headers;
    headers["Content-Type"] = "text/html";
    headers["Content-Length"] = to_string(content.length());
    headers.insert(make_pair("Location", location));

    return generate_response("302 Found", headers, content);
}


class AdminServer {
public:
    AdminServer(int port, AdminConsole& admin_console) : port(port), admin_console(admin_console) {
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
        if (listen(server_socket, 100) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }
        // init backend ports
        int i = 0;
        for (const auto& partition : admin_console.partitions) {
            for (const auto & node : partition.nodes) {
                backend_ports[i++] = node.addr;
            }
        }
    }

    ~AdminServer() { close(server_socket); }

    // admin
    string handle_post_admin(const string& raw_request) {
        // Parse the request headers
        std::unordered_map<std::string, std::string> headers;
        std::istringstream iss(raw_request);
        std::string line;
        while (std::getline(iss, line) && !line.empty()) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                headers[line.substr(0, pos)] = line.substr(pos + 2);
            }
        }

        // Read the request body
        std::string body;
        size_t pos = raw_request.find("\r\n\r\n");
        if (pos == std::string::npos) {
            // No empty line found, the request has no message body
            std::cout << "No message body found\n";
        } else {
            // Extract the message body substring
            body = raw_request.substr(pos + 4);
            std::cout << "Message body: " << body << "\n";
        }

        // Parse the request body as x-www-form-urlencoded data
        std::unordered_map<std::string, std::string> params;
        std::istringstream iss2(body);
        std::string pair;
        while (std::getline(iss2, pair, '&')) {
            // std::cerr<<"pair: "<<pair<<std::endl;
            size_t pos = pair.find('=');
            if (pos != std::string::npos) {
                std::string key = pair.substr(0, pos);
                std::string value = pair.substr(pos + 1);
                params[key] = value;
            }
        }

        // Extract the username and password values
        std::string serverType = params["serverType"];
        std::string action = params["action"];
        std::string serverIP_index = params["serverId"];
        cerr<<"serverIP_idx: " << serverIP_index<<endl;

        // Print the results
        std::cout << "serverType: " << serverType << std::endl;
        std::cout << "action: " << action << std::endl;

    	// ask for master to connect to a kv server
    	string response;

    	if (serverType == "backend") {
			std::string serverIP = backend_ports[stoi(serverIP_index)];
    		if (action == "shutdown") { // shutdown backend server
    			if (admin_console.adminshutdown(serverIP)) {
    				response = "Backend node " + serverIP + " successfully shutdown.";
    			} else {
    				response = "Failed to shutdown backend node " + serverIP + ".";
    			}
    		} else if (action == "restart") { // restart backend server
    			if (admin_console.adminstartup(serverIP)) {
                    response = "Backend node " + serverIP + " successfully started.";
                } else {
                    response = "Failed to start backend node " + serverIP + ".";
                }
    		}
    	} else { // "frontend"
    		std::string serverIP = frontend_ports[stoi(serverIP_index)];
			if (action == "shutdown") { // shutdown backend server
				cerr<<"debug2\n";
				if (admin_console.disable_frontend(serverIP)) {
					cerr<<"debug3\n";
					response = "Frontend node " + serverIP + " successfully shutdown.";
				} else {
					cerr<<"debug4\n";
					response = "Failed to shutdown frontend node " + serverIP + ".";
				}
			}
			else if (action == "restart") {
				if (admin_console.restart_frontend(serverIP)) {
					response = "Frontend node " + serverIP + " successfully restart.";
				} else {
					response = "Failed to restart frontend node " + serverIP + ".";
				}
			}
    	}

    	unordered_map<string, string> resposeHeaders;
    	resposeHeaders.insert(make_pair("Content-Type", "text/plain"));
    	resposeHeaders.insert(make_pair("Content-Length", to_string(response.size())));

    	return generate_response("200 OK", resposeHeaders, response);

    //    return serve_webpage();
    }

    // admin
	string handle_post_status(const string& raw_request) {
		vector<bool> res = admin_console.do_healthcheck();
		vector<bool> frontend_res = admin_console.check_frontend_status();
		// Parse the request headers
		std::unordered_map<std::string, std::string> headers;
		std::istringstream iss(raw_request);
		std::string line;
		while (std::getline(iss, line) && !line.empty()) {
			size_t pos = line.find(':');
			if (pos != std::string::npos) {
				headers[line.substr(0, pos)] = line.substr(pos + 2);
			}
		}

		// Read the request body
		std::string body;
		size_t pos = raw_request.find("\r\n\r\n");
		if (pos == std::string::npos) {
			// No empty line found, the request has no message body
			std::cout << "No message body found\n";
		} else {
			// Extract the message body substring
			body = raw_request.substr(pos + 4);
			std::cout << "Message body: " << body << "\n";
		}

		// Parse the request body as x-www-form-urlencoded data
		std::unordered_map<std::string, std::string> params;
		std::istringstream iss2(body);
		std::string pair;
		while (std::getline(iss2, pair, '&')) {
			// std::cerr<<"pair: "<<pair<<std::endl;
			size_t pos = pair.find('=');
			if (pos != std::string::npos) {
				std::string key = pair.substr(0, pos);
				std::string value = pair.substr(pos + 1);
				params[key] = value;
			}
		}

		// Extract the username and password values
		std::string serverType = params["serverType"];
		std::string serverIP_ID = params["serverId"];
//		cerr<<"serverIP_idx: " << serverIP_index<<endl;

		int serverIP_index = stoi(serverIP_ID);

		// Print the results
		std::cout << "serverType: " << serverType << std::endl;

		// ask for master to connect to a kv server
		string response = "";

		if (serverType == "backend") {
			if (res[serverIP_index]) {
				cerr<<"true\n";
				response = "200 OK";
			} else {
				cerr<<"false\n";
				response = "503 Service Unavailable";
			}
		} else { // "frontend"
			if (frontend_res[serverIP_index]) {
				response = "200 OK";
			} else {
				response = "503 Service Unavailable";
			}
		}

		unordered_map<string, string> resposeHeaders;
		resposeHeaders.insert(make_pair("Content-Type", "text/plain"));
		resposeHeaders.insert(make_pair("Content-Length", to_string(response.size())));

		return generate_response("200 OK", resposeHeaders, response);

	//    return serve_webpage();
	}

	string handle_post_details(const string& raw_request) {
		// Parse the request headers
		std::unordered_map<std::string, std::string> headers;
		std::istringstream iss(raw_request);
		std::string line;
		while (std::getline(iss, line) && !line.empty()) {
			size_t pos = line.find(':');
			if (pos != std::string::npos) {
				headers[line.substr(0, pos)] = line.substr(pos + 2);
			}
		}
		cerr<<"raw_request: " << raw_request<<endl;

		// Read the request body
		std::string body;
		size_t pos = raw_request.find("\r\n\r\n");
		if (pos == std::string::npos) {
			// No empty line found, the request has no message body
			std::cout << "No message body found\n";
		} else {
			// Extract the message body substring
			body = raw_request.substr(pos + 4);
			std::cout << "Message body: " << body << "\n";
		}

		// Parse the request body as x-www-form-urlencoded data
		std::unordered_map<std::string, std::string> params;
		std::istringstream iss2(body);
		std::string pair;
		while (std::getline(iss2, pair, '&')) {
			// std::cerr<<"pair: "<<pair<<std::endl;
			size_t pos = pair.find('=');
			if (pos != std::string::npos) {
				std::string key = pair.substr(0, pos);
				std::string value = pair.substr(pos + 1);
				params[key] = value;
			}
		}

		// Extract the username and password values
		std::string row = params["key"];
		std::string col = params["value"];

		// ask for master to connect to a kv server
		unordered_map<string, string> responseHeaders;
		string val = "";

		try { // get response from kv master (with password)
			val = kvClient->get(row, col);
			cout << "val=" << val << endl;
		} catch (KVNotFound &err) { // user does not exist
									//			cout << err.what() << endl;

			string content = "cannot get value";
			responseHeaders.insert(make_pair("Content-Type", "text/plain"));
			responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));

			return generate_response("404 Not found", responseHeaders, content);
		}

		responseHeaders.insert(make_pair("Content-Type", "text/plain"));
		responseHeaders.insert(make_pair("Content-Length", to_string(val.size())));

		return generate_response("200 OK", responseHeaders, val);

	}


    static void* health_check_loop(void* arg) {
        AdminConsole* admin_console = (AdminConsole*) arg;
        while (true) {
            // Perform health checks
            vector<bool> res = admin_console->do_healthcheck();
            vector<bool> frontend_res = admin_console->check_frontend_status();
//            admin_console->check_frontend_status();
            // Wait 10 seconds before performing next health check
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        return NULL;
    }

    void run() {
        // Create health check thread
        pthread_t health_check_thread;
        pthread_create(&health_check_thread, NULL, health_check_loop, &admin_console);

        while (true) {
            // Accept incoming connections and handle requests
            int client_socket = accept(server_socket, NULL, NULL);
            thread(&AdminServer::handle_client, this, client_socket).detach();

//            int client_socket = accept(server_socket, NULL, NULL);
//			ThreadArgs *args = new ThreadArgs(this, client_socket);
//			pthread_t client_thread;
//			pthread_create(&client_thread, nullptr, &AdminServer::handle_client, args);
//			pthread_detach(client_thread);
        }
    }

private:
    int server_socket;
    int port;
    AdminConsole& admin_console;
    std::map<int, string> frontend_ports{{0, "127.0.0.1:8080"},{1, "127.0.0.1:8081"},{2, "127.0.0.1:8082"}};
    std::map<int, string> backend_ports;

//    void handle_get_status(const Request &request, Response &response) {
//      std::string serverType = request.get_param_value("serverType");
//      int serverId = std::stoi(request.get_param_value("serverId"));
//      std::string action = request.get_param_value("action");
////      string raw_request =;
//
//      // Get the server status based on serverType and serverId
////      response = handle_post_admin(raw_request);
//
////      // Send the status back to the client as a JSON response
////      json j = {{"status", server_status}};
////      response.set_content(j.dump(), "application/json");
//    }

    void handle_client(int client_socket) {
        char buffer[1024] = {0};
        ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read < 0) {
            perror("recv");
            close(client_socket);
            return;
        }
        string raw_request(buffer, bytes_read);

        string response = "";

        // Extract the request line and headers
		stringstream ss(raw_request);
		string request_line, raw_headers;
		getline(ss, request_line);
		getline(ss, raw_headers, '\n');

		// Parse the request line
		vector<string> request_parts = split(request_line, ' ');
		string method = request_parts[0];
		string url = request_parts[1];
		string version = request_parts[2];
//
//		// Parse the headers
		unordered_map<string, string> headers = parse_headers(raw_headers);

        if (method == "POST") {
        	cerr<<url<<endl;
        	if (url.find("action") != string::npos) {
        		cerr<<"action\n";
        		response = handle_post_admin(raw_request);
        	} else if (url.find("status") != string::npos) {
        		cerr<<"status\n";
        		response = handle_post_status(raw_request);
        	} else if (url.find("details") != string::npos) {
        		cerr<<"details\n";
        		response = handle_post_details(raw_request);
        		send(client_socket, response.data(), response.size(), 0);
        		close(client_socket);
        	}
        } else if (method == "GET") {
        	if (url == "/" || url == "") {

				response = serve_webpage();

				send(client_socket, response.data(), response.size(), 0);

				close(client_socket);
        	} else if (url == "/key_value") {
        	    // Convert the vector to a JSON array
        	    vector<pair<string, string>> data = admin_console.displayAdminList();

        	    json j;
        	    for (const auto& p : data) {
        	        j.push_back({{"key", p.first}, {"value", p.second}});
        	        cerr<<"key: " << p.first;
        	        cerr<<"key: " << p.second;
        	    }
        	    std::string response_body = j.dump();

        	    // Create the HTTP response
        	    std::string response_headers = "HTTP/1.1 200 OK\r\n"
        	                                   "Content-Type: application/json\r\n"
        	                                   "Content-Length: " + std::to_string(response_body.size()) + "\r\n"
        	                                   "\r\n";

        	    // Write the response to the socket
        	    std::string response = response_headers + response_body;

        	    send(client_socket, response.data(), response.size(), 0);

        	    close(client_socket);
        	}
        }
        else {
            response = "Invalid request.";
        }

        unordered_map<string, string> responseHeaders;
        responseHeaders.insert(make_pair("Content-Type", "text/plain"));
        responseHeaders.insert(make_pair("Content-Length", to_string(response.size())));
        string finalResponse = generate_response("200 OK", responseHeaders, response);

        send(client_socket, finalResponse.data(), finalResponse.size(), 0);
        close(client_socket);
    }
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <KV config file>\n", argv[0]);
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
    kvClient = new KVClient(master_addr);
    // parse each partition line
    string line;
    while (getline(f, line)) {
        cout << "[INIT] Found partition config" << endl;
        vector<MasterNodeView> nodes;
        size_t i = 0, j = 0;
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

    AdminConsole admin_console(partitions);

    AdminServer server(8888, admin_console);
    server.run();

    return 0;
}
