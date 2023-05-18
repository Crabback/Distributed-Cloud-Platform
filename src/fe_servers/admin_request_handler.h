#pragma once

#include <unordered_map>
#include <vector>
#include <string>

using namespace std;

class AdminRequestHandler {
public:
    string handle_request(const string& raw_request, vector<Partition> partitions);
    string generate_response(const string& status_code, const unordered_map<string, string>& headers, const string& content);

private:
    unordered_map<string, string> parse_headers(const string& raw_headers);
    string trim(const string& str);
    vector<string> split(const string& str, char delimiter);
    // jinwei
    string handle_post_login(const string& raw_request);
    string serve_webpage();
    string generate_redirect(const string& location);
    // admin
    string handle_post_admin(const string& raw_request);
    bool disable_frontend(const std::string& node);
};
