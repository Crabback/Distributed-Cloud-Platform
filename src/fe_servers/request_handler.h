#pragma once

#include "session_manager.h"
#include "src/kv/client.h"
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

class RequestHandler {
  public:
    //	RequestHandler(std::unique_ptr<bool> serving) : serving_(std::move(serving)) {}
    RequestHandler(SessionManager *session_manager, KVClient *kvClient, bool &serving_) : session_manager_(session_manager), client(kvClient), serving(serving_) {}

    string handle_request(const string &raw_request);
    string generate_response(const string &status_code, const unordered_map<string, string> &headers, const string &content);

  private:
    //    unique_ptr<bool> serving_;
    KVClient *client;
    bool &serving;
    SessionManager *session_manager_; // session manager

    unordered_map<string, string> parse_headers(const string &raw_headers);
    string trim(const string &str);
    vector<string> split(const string &str, char delimiter);
    string join(const vector<std::string> &vec, const string &delimiter);
    string serve_webpage(const string &location);
    string serve_image(const string &location);
    unordered_map<string, string> parse_json(const string &body);
    //    string generate_redirect(const string& location);
    // admin
    //    string handle_post_admin(const string& raw_request);
    string handle_get_webmail(const string &raw_request, string url, string box, unordered_map<string, string> &headers);
    string handle_post_webmail(const string &raw_request, string url, unordered_map<string, string> &headers);
    string handle_delete_webmail(const string &raw_request, string url, unordered_map<string, string> &headers);

    string handle_get_storage(const string &raw_request, string url, string action, unordered_map<string, string> &headers);
    string handle_post_storage(const string &raw_request, string url, string action, unordered_map<string, string> &headers);
    string handle_delete_storage(const string &raw_request, string url, bool is_folder, unordered_map<string, string> &headers);

    string handle_post_login(const string &raw_request);
    string handle_post_accounts(const string &raw_request, string url);
    string get_cookie_from_header(const unordered_map<string, string> &headers);
};
