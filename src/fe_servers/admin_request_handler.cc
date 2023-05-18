#include "admin_request_handler.h"
#include "src/kv/master_client.h"
#include "admin_console.h"
#include <sstream>
#include <algorithm>
#include <iterator>
#include <regex>
#include <unordered_map>
#include <vector>
#include <string>
// Jinwei
#include <iostream>
#include <fstream>

using namespace std;

string AdminRequestHandler::handle_request(const string& raw_request, vector<MasterPartitionView> partitions) {
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

    // Parse the headers
    unordered_map<string, string> headers = parse_headers(raw_headers);
    
    // //Jinwei
    // std::cerr << raw_request << std::endl;

    // Call the appropriate handler function based on the request method and URL
    if (method == "GET") {
        if (url == "/") {
        	// respond client with a login page
        	return generate_redirect("file:///home/cis5050/git/T02/fe_pages/login/PennCloud_login_username.html");
        } else if (url == "/hello") {
    		string content = "Hello, world!";
			unordered_map<string, string> headers;
			headers.insert(make_pair("Content-Type", "text/plain"));
			headers.insert(make_pair("Content-Length", to_string(content.size())));
			return generate_response("200 OK", headers, content);
    	} else if (url == "/admin") { // TODO
    		// authentication check TODO
    		return generate_redirect("file:///home/cis5050/git/T02/fe_pages/admin.html");
    	}
    } else if (method == "POST") {
    	if (url == "/login") {
    		return handle_post_login(raw_request);
    	} else if (url == "/admin") {
    		return handle_post_admin(raw_request);
    	}
    } else if (method == "DELE") {

    } else {
    	// Return a 404 Not Found response for unrecognized requests
		string content = "404 Not Found";
		unordered_map<string, string> headers;
		headers.insert(make_pair("Content-Type", "text/plain"));
		headers.insert(make_pair("Content-Length", to_string(content.size())));
		return generate_response("404 Not Found", headers, content);
    }
}

string AdminRequestHandler::generate_response(const string& status_code, const unordered_map<string, string>& headers, const string& content) {
    ostringstream response;
    response << "HTTP/1.1 " << status_code << "\r\n";
    unordered_map<string, string> headers_copy = headers; // Make a copy of headers to modify
    headers_copy["Content-Type"] = "text/plain";
    headers_copy["Content-Length"] = to_string(response.size());
    for (const auto& header : headers_copy) {
        response << header.first << ": " << header.second << "\r\n";
    }
    response << "\r\n" << content;
    return response.str();
}

unordered_map<string, string> AdminRequestHandler::parse_headers(const string& raw_headers) {
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

string AdminRequestHandler::trim(const string& str) {
    string whitespace = " \t\r\n";
    size_t start = str.find_first_not_of(whitespace);
    size_t end = str.find_last_not_of(whitespace);
    if (start == string::npos || end == string::npos) {
        return "";
    }
    return str.substr(start, end - start + 1);
}

bool AdminRequestHandler::disable_frontend(const std::string& node) {
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
        string request = "GET /shutdown HTTP/1.0\r\n\r\n";

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
            // Check if the response starts with "HTTP/1.0 200 OK"
            bool success = (response.find("HTTP/1.0 503 Service Unavailable") == 0);
            // Log the server IP and the status
            cout << "Frontend server " << node << " disable status: " << "HTTP/1.0 200 OK" << endl;
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

vector<string> AdminRequestHandler::split(const string& str, char delimiter) {
    vector<string> tokens;
    istringstream ss(str);
    string token;

    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}


string AdminRequestHandler::serve_webpage() {
    // Read the contents of the HTML file
    ifstream file("src/fe_pages/webmail.html");
    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    // Set the response headers
    unordered_map<string, string> headers;
    headers["Content-Type"] = "text/html";
    headers["Content-Length"] = to_string(content.length());

    // Generate the response
    string response = generate_response("200 OK", headers, content);

    return response;
}

string AdminRequestHandler::generate_redirect(const string& location) {
    string content = "";
    unordered_map<string, string> headers;
    headers["Content-Type"] = "text/html";
    headers["Content-Length"] = to_string(content.length());
    headers.insert(make_pair("Location", location));

    return generate_response("302 Found", headers, content);
}

// admin
string AdminRequestHandler::handle_post_admin(const string& raw_request) {
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
    std::string serverIP = params["serverIP"];

    // Print the results
    std::cout << "serverType: " << serverType << std::endl;
    std::cout << "action: " << action << std::endl;
    std::cout << "serverIP: " << serverIP << std::endl;

	// ask for master to connect to a kv server
	//KVMasterClient masterClient(partitions);
//	AdminConsole* admin_console = (AdminConsole*) masterClient(partitions);
	string response;

	if (serverType == "frontend") { // "frontend"
		cerr<<"debug1\n";
		if (action == "shutdown") { // shutdown backend server
			cerr<<"debug2\n";
			if (disable_frontend(serverIP)) {
				cerr<<"debug3\n";
				response = "Frontend node " + serverIP + " successfully shutdown.";
			} else {
				cerr<<"debug4\n";
				response = "Failed to shutdown frontend node " + serverIP + ".";
			}
		}
//		else { // restart backend server
//			if (admin_console->restart_frontend(serverIP)) {
//				response = "Frontend node " + serverIP + " successfully started.";
//			} else {
//				response = "Failed to start frontend node " + serverIP + ".";
//			}
//		}
	}

	unordered_map<string, string> resposeHeaders;
	resposeHeaders.insert(make_pair("Content-Type", "text/plain"));
	resposeHeaders.insert(make_pair("Content-Length", to_string(content.size())));

	return generate_response("200 OK", resposeHeaders, response);

//    return serve_webpage();
}
