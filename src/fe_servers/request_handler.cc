#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits.h>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "../webmail/webmail_utils.h"
#include "request_handler.h"

using namespace std;

string RequestHandler::handle_request(const string &raw_request) {
    // Extract the request line and headers
    cout << "Request Handler: \n" << raw_request << endl;
    // Get the current working directory
    char buffer[PATH_MAX];
    getcwd(buffer, sizeof(buffer));
    string working_dir(buffer);

    stringstream ss(raw_request);
    string request_line = "";
    string raw_headers = "";
    getline(ss, request_line);
    //    getline(ss, raw_headers, '\n');

    // Parse the request line
    vector<string> request_parts = split(request_line, ' ');
    string method = request_parts[0];
    string url = request_parts[1];
    string version = request_parts[2];

    // Parse the headers
    unordered_map<string, string> headers;
    string header_line = "";
    while (getline(ss, header_line, '\r')) {
        // Skip the '\n' character after the '\r'
        ss.ignore(1);
        // Split the header line into name and value
        size_t separator_pos = header_line.find(':');
        if (separator_pos != string::npos) {
            string key = header_line.substr(0, separator_pos);
            string value = header_line.substr(separator_pos + 2); // Skip the ':' and space
            headers[key] = value;
            // cout << "parse_headers{" << key << " : " << value << "}" << endl;
        }
    }

    if (!serving) {
        if (method == "GET") {
            if (url.substr(0, 7) == "/status") {
                string content = "503 Service Unavailable\n";
                unordered_map<string, string> headers;
                headers.insert(make_pair("Content-Type", "text/plain"));
                headers.insert(make_pair("Content-Length", to_string(content.size())));
                return generate_response("503 Service Unavailable", headers, content);
            } else if (url.substr(0, 8) == "/restart") {
                serving = true;
                string content = "Successfully restart server";
                unordered_map<string, string> headers;
                headers.insert(make_pair("Content-Type", "text/plain"));
                headers.insert(make_pair("Content-Length", to_string(content.size())));
                return generate_response("200 OK", headers, content);
            }
        }
    } else {
        // Call the appropriate handler function based on the request method and URL
        /************************ GET REQUESTS *************************/
        if (method == "GET") {
            /************ request from client *************/
            if (url.length() <= 1 && (url == "/" || url == "")) {
                //				// change (jinwei):
                //				string sid = get_cookie_from_header(headers);
                //				cerr<<"sid: "<< sid<<endl;
                //				if (session_manager_->session_check(sid)) { // session is valid
                //					// redirect to home page
                //					return serve_webpage(working_dir + "/fe_pages/homepage.html");
                //				} else { // session is not valid
                //					// respond client with a login page
                //					return serve_webpage(working_dir + "/fe_pages/login.html");
                //				}
                return serve_webpage(working_dir + "/fe_pages/homepage.html");
                // // respond client with a login page
                // return serve_webpage(working_dir + "/fe_pages/login.html");
            } else if (url.substr(0, 7) == "/images") {
                // respond client with a login page
                return serve_image(working_dir + "/fe_pages" + url);

            } else if (url.substr(0, 6) == "/hello") {
                // change (jinwei):
                string sid = get_cookie_from_header(headers);
                string content = "";
                if (session_manager_->session_check(sid)) { // session is valid
                    unordered_map<string, string> session_data = session_manager_->get_session(sid);
                    // respond client with username
                    content = session_data["username"];
                } else { // session is not valid
                    // respond client with a login page
                    content = "401 Unauthorized";
                }

                unordered_map<string, string> headers;
                headers.insert(make_pair("Content-Type", "text/plain"));
                headers.insert(make_pair("Content-Length", to_string(content.size())));
                return generate_response("200 OK", headers, content);

            } else if (url.substr(0, 6) == "/admin") { // TODO
                                                       //                // authentication check (jinwei)
                                                       //                string sid = get_cookie_from_header(headers);
                                                       //                if (!session_manager_->session_check(sid)) { // session is valid
                                                       //                    // respond client with a login page
                                                       //                    return serve_webpage(working_dir + "/fe_pages/login.html");
                                                       //                }
                string content = "200 OK";
                unordered_map<string, string> headers;
                headers.insert(make_pair("Content-Type", "text/plain"));
                headers.insert(make_pair("Content-Length", to_string(content.size())));
                return generate_response("200 OK", headers, content);

                //                return serve_webpage(working_dir + "/fe_pages/admin.html");

            } else if (url.substr(0, 6) == "/login") {
                // respond client with a login page
                return serve_webpage(working_dir + "/fe_pages/login.html");

            } else if (url.substr(0, 7) == "/logout") {
                // authentication check (jinwei)
                string sid = get_cookie_from_header(headers);
                if (session_manager_->session_check(sid)) { // session is valid
                    // delete sid
                    session_manager_->delete_session(sid);
                    //					// respond client with a home page
                    //					return serve_webpage(working_dir + "/fe_pages/homepage.html");
                }
                // respond client with a home page
                return serve_webpage(working_dir + "/fe_pages/homepage.html");

            } else if (url.substr(0, 9) == "/register") {
                return serve_webpage(working_dir + "/fe_pages/register.html");

            } else if (url.substr(0, 15) == "/reset_password") {
                return serve_webpage(working_dir + "/fe_pages/reset_password.html");

            } else if (url.substr(0, 5) == "/home") {
                // authentication check
                return serve_webpage(working_dir + "/fe_pages/homepage.html");

                /************************ request from webmail *************************/
            } else if (url.substr(0, 8) == "/webmail") {
                // authentication check (jinwei)
                string sid = get_cookie_from_header(headers);
                if (!session_manager_->session_check(sid)) { // session is valid
                    // respond client with a login page
                    return serve_webpage(working_dir + "/fe_pages/login.html");
                }
                return serve_webpage(working_dir + "/fe_pages/webmail.html");

            } else if (url.substr(0, 6) == "/inbox") {
                return handle_get_webmail(raw_request, url, "inbox", headers);

            } else if (url.substr(0, 8) == "/deleted") {
                return handle_get_webmail(raw_request, url, "deleted", headers);

            } else if (url.substr(0, 5) == "/sent") {
                return handle_get_webmail(raw_request, url, "sent", headers);

                /************************ request from storage *************************/
            } else if (url.substr(0, 8) == "/storage") {
                // authentication check (jinwei)
                string sid = get_cookie_from_header(headers);
                if (!session_manager_->session_check(sid)) { // session is valid
                    // respond client with a login page
                    return serve_webpage(working_dir + "/fe_pages/login.html");
                }
                return serve_webpage(working_dir + "/fe_pages/storage.html");

            } else if (url.substr(0, 6) == "/files") {
                return handle_get_storage(raw_request, url, "viewfile", headers);

            } else if (url.substr(0, 13) == "/downloadfile") {
                return handle_get_storage(raw_request, url, "downloadfile", headers);

                /************************ request from admin server *************************/
            } else if (url.substr(0, 9) == "/shutdown") { // from admin server
                serving = false;
                string content = "503 Service Unavailable\n";
                unordered_map<string, string> headers;
                headers.insert(make_pair("Content-Type", "text/plain"));
                headers.insert(make_pair("Content-Length", to_string(content.size())));
                return generate_response("503 Service Unavailable", headers, content);

            } else if (url.substr(0, 7) == "/status") {
                string content = "OK STATUS\n";
                unordered_map<string, string> headers;
                headers.insert(make_pair("Content-Type", "text/plain"));
                headers.insert(make_pair("Content-Length", to_string(content.size())));
                return generate_response("200 OK", headers, content);

            } else {
                // Return a 404 Not Found response for unrecognized requests
                string content = "404 GET Not Found";
                unordered_map<string, string> headers;
                headers.insert(make_pair("Content-Type", "text/plain"));
                headers.insert(make_pair("Content-Length", to_string(content.size())));
                return generate_response("404 Not Found", headers, content);
            }

            /************************ POST REQUESTS *************************/
        } else if (method == "POST") {
            if (url.substr(0, 9) == "/shutdown") {
                // serving = false;
                string content = "503 Service Unavailable\n";
                unordered_map<string, string> headers;
                headers.insert(make_pair("Content-Type", "text/plain"));
                headers.insert(make_pair("Content-Length", to_string(content.size())));
                return generate_response("503 Service Unavailable", headers, content);

                /************************ request from webmail *************************/
            } else if (url.substr(0, 7) == "/emails") {
                return handle_post_webmail(raw_request, url, headers);

            } else if (url.substr(0, 13) == "/deleteemails") {
                return handle_delete_webmail(raw_request, url, headers);

                /************************ request from storage *************************/
            } else if (url.substr(0, 11) == "/uploadfile") {
                return handle_post_storage(raw_request, url, "uploadfile", headers);

            } else if (url.substr(0, 13) == "/createfolder") {
                return handle_post_storage(raw_request, url, "createfolder", headers);

            } else if (url.substr(0, 9) == "/movefile") {
                return handle_post_storage(raw_request, url, "movefile", headers);

            } else if (url.substr(0, 11) == "/movefolder") {
                return handle_post_storage(raw_request, url, "movefolder", headers);

            } else if (url.substr(0, 7) == "/rename") {
                return handle_post_storage(raw_request, url, "rename", headers);

            } else if (url.substr(0, 13) == "/deletefolder") {
                return handle_delete_storage(raw_request, url, true, headers);

            } else if (url.substr(0, 13) == "/deletefile") {
                return handle_delete_storage(raw_request, url, false, headers);

                /************************ request from login *************************/
            } else if (url == "/login" || url == "/register" || url == "/reset_password") {
                return handle_post_accounts(raw_request, url);

            } else {
                string content = "404 POST Not Found";
                unordered_map<string, string> headers;
                headers.insert(make_pair("Content-Type", "text/plain"));
                headers.insert(make_pair("Content-Length", to_string(content.size())));
                return generate_response("404 Not Found", headers, content);
            }

            /************************ DELETE REQUESTS *************************/
        } else if (method == "DELE") {
            // (jinwei)
            // Return a 404 Not Found response for unrecognized requests
            string content = "404 DELETE Not Found";
            unordered_map<string, string> headers;
            headers.insert(make_pair("Content-Type", "text/plain"));
            headers.insert(make_pair("Content-Length", to_string(content.size())));
            return generate_response("404 Not Found", headers, content);

            /************************ OPTIONS REQUESTS *************************/
        } else {
            // Return a 404 Not Found response for unrecognized requests
            string content = "404 Not Found";
            unordered_map<string, string> headers;
            headers.insert(make_pair("Content-Type", "text/plain"));
            headers.insert(make_pair("Content-Length", to_string(content.size())));
            return generate_response("404 Not Found", headers, content);
        }
    }
}

string RequestHandler::generate_response(const string &status_code, const unordered_map<string, string> &headers, const string &content) {
    ostringstream response;
    response << "HTTP/1.1 " << status_code << "\r\n";
    unordered_map<string, string> headers_copy = headers; // Make a copy of headers to modify
    for (const auto &header : headers_copy) {
        response << header.first << ": " << header.second << "\r\n";
    }
    response << "\r\n" << content;
    return response.str();
}

unordered_map<string, string> RequestHandler::parse_headers(const string &raw_headers) {
    unordered_map<string, string> headers;

    stringstream ss_headers(raw_headers);
    string header_line;
    while (getline(ss_headers, header_line, '\r')) {
        // remove the '\n' character from the end of the string
        if (!header_line.empty() && header_line.back() == '\n') {
            header_line.pop_back();
        }
        header_line = trim(header_line);
        if (header_line.empty()) {
            continue;
        }
        size_t delim_pos = header_line.find(':');
        if (delim_pos != string::npos) {
            string header_key = header_line.substr(0, delim_pos);
            string header_value = header_line.substr(delim_pos + 1);
            headers[header_key] = header_value;
            cerr << "parse_headers: key:" << header_key << endl;
            cerr << "parse_headers: value:" << header_value << endl;
        }
    }

    return headers;
}

string RequestHandler::trim(const string &str) {
    string whitespace = " \t\r\n";
    size_t start = str.find_first_not_of(whitespace);
    size_t end = str.find_last_not_of(whitespace);
    if (start == string::npos || end == string::npos) {
        return "";
    }
    return str.substr(start, end - start + 1);
}

vector<string> RequestHandler::split(const string &str, char delimiter) {
    vector<string> tokens;
    istringstream ss(str);
    string token;
    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

string RequestHandler::join(const vector<std::string> &vec, const string &delimiter) {
    ostringstream os;
    for (const auto &str : vec) {
        os << str;
        if (&str != &vec.back()) {
            os << delimiter;
        }
    }
    return os.str();
}
/**
 * Jinwei
 */
string RequestHandler::handle_post_accounts(const string &raw_request, string url) {
    // Parse the request headers
    unordered_map<string, string> headers;
    istringstream iss(raw_request);
    string line = "";
    while (getline(iss, line) && !line.empty()) {
        size_t pos = line.find(':');
        if (pos != string::npos) {
            headers[line.substr(0, pos)] = line.substr(pos + 2);
        }
    }

    // Read the request body
    string body = "";
    size_t pos = raw_request.find("\r\n\r\n");
    if (pos == string::npos) {
        // No empty line found, the request has no message body
        cout << "No message body found\n";
    } else {
        // Extract the message body substring
        body = raw_request.substr(pos + 4);
        cout << "Message body: " << body << "\n";
    }

    // Parse the request body as x-www-form-urlencoded data
    unordered_map<string, string> params;
    istringstream iss2(body);
    string pair;
    while (getline(iss2, pair, '&')) {
        // cerr<<"pair: "<<pair<<endl;
        size_t pos = pair.find('=');
        if (pos != string::npos) {
            string key = pair.substr(0, pos);
            string value = pair.substr(pos + 1);
            params[key] = value;
        }
    }

    // Extract the username and password values
    string username = params["username"];
    string password = params["password"];

    // Print the results
    cout << "Username: " << username << endl;
    cout << "Password: " << password << endl;

    // string content = "Username: " + username + ", Password: " + password;
    /** communicate with KV store **/
    // ask for master to connect to a kv server
    //    KVClient client("127.0.0.1:5000");

    // request get (username, "password")
    string passwordFromKV;
    unordered_map<string, string> responseHeaders;
    string content = "";

    if (url == "/login") {
        try { // get response from kv master (with password)
            passwordFromKV = client->get(username, "password");
            cout << "GET password=" << passwordFromKV << endl;
        } catch (KVNotFound &err) { // user does not exist
                                    //			cout << err.what() << endl;

            content = "User does not exist";
            responseHeaders.insert(make_pair("Content-Type", "text/plain"));
            responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));

            return generate_response("200 OK", responseHeaders, content);
        }

        // check if the password matches and respond to the client
        if (passwordFromKV == password) { // password is correct
                                          // add to cookie TODO
                                          // generate session id
            string sid = session_manager_->create_session(username);
            string cookie_value = session_manager_->get_cookie_value(sid);

            // respond to client
            content = "Successful login";
            responseHeaders.insert(make_pair("Content-Type", "text/plain"));
            responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));
            responseHeaders.insert(make_pair("Set-Cookie", cookie_value));

            return generate_response("200 OK", responseHeaders, content);
        }

        // wrong password
        content = "Incorrect password";

    } else if (url == "/register") {
        try { // get response from kv master (with password)
            client->put(username, "password", password);
            cout << "PUT password=" << password << endl;
        } catch (KVError &err) { // register failed
            cout << err.what() << endl;

            content = "Register failed";
            responseHeaders.insert(make_pair("Content-Type", "text/plain"));
            responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));

            return generate_response("200 OK", responseHeaders, content);
        }
        // successfully reset the password
        content = "Successfully register";

    } else if (url == "/reset_password") {
        string new_password = params["new_password"];

        try { // get response from kv master (with password)
            client->cput(username, "password", password, new_password);
            cout << "RESET " << password << " to" << new_password << endl;
        } catch (KVError &err) { // user does not exist
            cout << err.what() << endl;

            string content = "Reset password failed\n";
            responseHeaders.insert(make_pair("Content-Type", "text/plain"));
            responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));

            return generate_response("200 OK", responseHeaders, content);
        }
        // successfully reset the password
        content = "Successfully reset the password";

        // delete previous session id for the user
        string sid = get_cookie_from_header(headers);
        if (session_manager_->session_check(sid)) { // session is valid
            // delete sid
            session_manager_->delete_session(sid);
        }
    }
    responseHeaders.insert(make_pair("Content-Type", "text/plain"));
    responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));

    return generate_response("200 OK", responseHeaders, content);

    //    return serve_webpage();
}

string RequestHandler::handle_get_webmail(const string &raw_request, string url, string box, unordered_map<string, string> &headers) {
    string content;
    unordered_map<string, string> responseHeaders;
    string sid = get_cookie_from_header(headers);
    if (session_manager_->session_check(sid)) { // session is valid
        unordered_map<string, string> session_data = session_manager_->get_session(sid);
        string username = session_data["username"];
        try {
            string uid_list_str = client->get(username, box);
            vector<string> uid_list = split(uid_list_str, ',');
            for (int i = 0; i < (int)uid_list.size(); i++) {
                string uid = uid_list[i];
                content += "UID: " + uid + "\n";
                content += client->get(username, uid);
                content += "====================\n";
            }
        } catch (KVError &err) {
            cout << err.what() << endl;
        }
    } else { // session is not valid
        content = "401 Unauthorized";
    }
    responseHeaders.insert(make_pair("Access-Control-Allow-Origin", "*"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Methods", "POST, GET, OPTIONS"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Headers", "Content-Type"));
    responseHeaders.insert(make_pair("Content-Type", "text/plain"));
    responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));
    return generate_response("200 OK", responseHeaders, content);
}

string RequestHandler::handle_post_webmail(const string &raw_request, string url, unordered_map<string, string> &headers) {
    string body = "";
    size_t pos = raw_request.find("\r\n\r\n");
    if (pos == string::npos) {
        cout << "No message body found" << endl;
    } else {
        body = raw_request.substr(pos + 4);
        cout << "Message body: " << body << "\n";
    }

    unordered_map<string, string> params = parse_json(body);
    unordered_map<string, string> responseHeaders;
    string content = "Successfully sent!";
    regex pattern(R"(\b[A-Za-z0-9._%+-]+@(?:[A-Za-z0-9.-]+\.[A-Za-z]{2,}|localhost)\b)");
    if (regex_match(params["to"], pattern)) {
        string sid = get_cookie_from_header(headers);
        if (session_manager_->session_check(sid)) { // session is valid
            unordered_map<string, string> session_data = session_manager_->get_session(sid);
            string username = session_data["username"];
            string email_body = params["subject"] + "\n" + params["body"] + "\n";
            send_to_webmail(params["to"], username + "@penncloud.com", email_body);
        } else { // session is not valid
            content = "401 Unauthorized";
        }
    } else {
        content = "402 Recipient not found";
    }
    responseHeaders.insert(make_pair("Access-Control-Allow-Origin", "*"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Methods", "POST, GET, OPTIONS"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Headers", "Content-Type"));
    responseHeaders.insert(make_pair("Content-Type", "text/plain"));
    responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));
    return generate_response("200 OK", responseHeaders, content);
}

string RequestHandler::handle_delete_webmail(const string &raw_request, string url, unordered_map<string, string> &headers) {
    string body = "";
    size_t pos = raw_request.find("\r\n\r\n");
    if (pos == string::npos) {
        cout << "No message body found" << endl;
    } else {
        body = raw_request.substr(pos + 4);
        cout << "Message body: " << body << "\n";
    }
    string uid_to_remove = body;
    unordered_map<string, string> responseHeaders;
    string content = "Successfully deleted email";
    string sid = get_cookie_from_header(headers);
    if (session_manager_->session_check(sid)) { // session is valid
        unordered_map<string, string> session_data = session_manager_->get_session(sid);
        string username = session_data["username"];
        while (1) {
            try {
                // string uid_list_old = "uid1,uid2,uid3";
                string uid_list_old = client->getdefault(username, "inbox", "");
                string uid_list_new = uid_list_old;
                pos = uid_list_new.find(uid_to_remove);
                if (pos != string::npos) {
                    uid_list_new.replace(pos, uid_to_remove.length(), "");
                }
                string doublecomma = ",,";
                pos = uid_list_new.find(doublecomma);
                if (pos != string::npos) {
                    uid_list_new.replace(pos, doublecomma.length(), ",");
                }
                if (uid_list_new.front() == ',') {
                    uid_list_new = uid_list_new.substr(1);
                }
                if (uid_list_new.back() == ',') {
                    uid_list_new.pop_back();
                }
                client->cput(username, "inbox", uid_list_old, uid_list_new);
                cout << "CPUT recipient=" << username << endl;
                cout << "uid_list_old: " << uid_list_old << endl;
                cout << "uid_list_new: " << uid_list_new << endl;
            } catch (KVNoMatch &err) {
                cout << "CPUT RPC race conditions: " << err.what() << endl;
                continue;
            } catch (KVError &err) {
                cout << "CPUT RPC failed: " << err.what() << endl;
            }
            break;
        }
        while (1) {
            try {
                string uid_list_old = client->getdefault(username, "deleted", "");
                string uid_list_new;
                if (uid_list_old == "") {
                    uid_list_new = uid_to_remove;
                } else {
                    uid_list_new = uid_list_old + "," + uid_to_remove;
                }
                client->cput(username, "deleted", uid_list_old, uid_list_new);
                cout << "CPUT recipient=" << username << endl;
            } catch (KVNoMatch &err) {
                cout << "CPUT RPC race conditions: " << err.what() << endl;
                continue;
            } catch (KVError &err) {
                cout << "CPUT RPC failed: " << err.what() << endl;
            }
            break;
        }
        // try {
        //     client->del(username, uid_to_remove);
        // } catch (KVError &err) {
        //     cout << err.what() << endl;
        // }
    } else { // session is not valid
        content = "401 Unauthorized";
    }
    responseHeaders.insert(make_pair("Access-Control-Allow-Origin", "*"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Methods", "POST, GET, OPTIONS"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Headers", "Content-Type"));
    responseHeaders.insert(make_pair("Content-Type", "text/plain"));
    responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));
    return generate_response("200 OK", responseHeaders, content);
}

string RequestHandler::handle_get_storage(const string &raw_request, string url, string action, unordered_map<string, string> &headers) {
    string body = "";
    size_t pos = raw_request.find("\r\n\r\n");
    if (pos == string::npos) {
        cout << "No message body found" << endl;
    } else {
        body = raw_request.substr(pos + 4);
        cout << "Message body: " << body << "\n";
    }
    string file_to_download = body;
    string content;
    unordered_map<string, string> responseHeaders;
    string file_list_str;
    string sid = get_cookie_from_header(headers);
    if (session_manager_->session_check(sid)) { // session is valid
        unordered_map<string, string> session_data = session_manager_->get_session(sid);
        string username = session_data["username"];
        try {
            file_list_str = client->get(username, "storage");
        } catch (KVError &err) {
            cout << err.what() << endl;
        }
        // string file_list_str = "src/file1.c:k1+k2,src/web/file2.c:k1+k2+k3,file3.c:k1";
        cout << "file_list_str: " << file_list_str << endl;
        vector<string> file_list = split(file_list_str, ',');
        if (action == "downloadfile") {
            file_to_download = url.substr(url.find("?path=") + 6);
            try {
                for (int i = 0; i < (int)file_list.size(); i++) {
                    string file = file_list[i];
                    string keys;
                    string filepath;
                    size_t pos = file.find(":");
                    if (pos != string::npos) {
                        filepath = file.substr(0, pos);
                        if (filepath == file_to_download) {
                            keys = file.substr(pos + 1);
                            vector<string> keys_list = split(keys, '+');
                            for (int j = 0; j < (int)keys_list.size(); j++) {
                                string key = keys_list[j];
                                content += client->get(username + ":" + filepath, key);
                            }
                        }
                    }
                }
                int offset = file_to_download.find('/') == string::npos ? 0 : file_to_download.find_last_of('/') + 1;
                string filename = file_to_download.substr(offset);
                responseHeaders.insert(make_pair("Content-Disposition", "attachment; filename=\"" + filename + "\""));
            } catch (KVError &err) {
                cout << err.what() << endl;
            }
        } else { // retrieve file list
            vector<string> path_list;
            for (int i = 0; i < (int)file_list.size(); i++) {
                string file = file_list[i];
                size_t pos = file.find(":");
                string filepath;
                if (pos != string::npos) { // file
                    filepath = file.substr(0, pos);
                    path_list.push_back(filepath);
                } else { // empty folder
                    path_list.push_back(file);
                }
            }
            content = join(path_list, ",");
        }
    } else { // session is not valid
        content = "401 Unauthorized";
    }
    responseHeaders.insert(make_pair("Access-Control-Allow-Origin", "*"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Methods", "POST, GET, OPTIONS"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Headers", "Content-Type"));
    responseHeaders.insert(make_pair("Content-Type", "text/plain"));
    responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));
    return generate_response("200 OK", responseHeaders, content);
}

string RequestHandler::handle_post_storage(const string &raw_request, string url, string action, unordered_map<string, string> &headers) { // Read the request body
    // parse the request body
    string body = "";
    size_t pos = raw_request.find("\r\n\r\n");
    if (pos == string::npos) {
        cout << "No message body found" << endl;
    } else {
        body = raw_request.substr(pos + 4);
        cout << "Message body: " << body << "\n";
    }
    unordered_map<string, string> params = parse_json(body);
    unordered_map<string, string> responseHeaders;
    string content;

    // get the logged in username
    string sid = get_cookie_from_header(headers);
    if (session_manager_->session_check(sid)) { // session is valid
        unordered_map<string, string> session_data = session_manager_->get_session(sid);
        string username = session_data["username"];

        // string file_list_old = "src/file1.c:k1+k2,src/web/file2.c:k1+k2+k3,file3.c:k1";
        if (action == "createfolder") {
            content = "Successfully created folder " + params["folderName"] + "!";
            string new_folder = params["folderName"] + "/";
            while (1) {
                try { // get the file list and append to it (folder has no key)
                    string file_list_old = client->getdefault(username, "storage", "");
                    string file_list_new;

                    // check duplicates
                    vector<string> file_list = split(file_list_old, ',');
                    set<string> unique_files(file_list.begin(), file_list.end());
                    if (unique_files.find(new_folder) != unique_files.end()) {
                        content = "Failed created folder: folder existed!";
                        break;
                    }

                    if (file_list_old == "") {
                        file_list_new = new_folder;
                    } else {
                        file_list_new = file_list_old + "," + new_folder;
                    }
                    client->cput(username, "storage", file_list_old, file_list_new);
                    cout << "CPUT recipient=" << username << endl;
                } catch (KVNoMatch &err) {
                    cout << "CPUT RPC race conditions: " << err.what() << endl;
                    continue;
                } catch (KVError &err) {
                    cout << "CPUT RPC failed: " << err.what() << endl;
                }
                break;
            }

        } else if (action == "movefolder") {
            // example: old path: src/web3/, target path: src/web2/web3/
            content = "Successfully moved folder";
            cout << params["old_path"] << " to " << params["target_path"] << endl;
            while (1) {
                try {
                    string file_list_old = client->getdefault(username, "storage", "");
                    string file_list_new = file_list_old;
                    vector<string> paths = split(file_list_new, ',');
                    string target_path = params["target_path"];
                    string old_path = params["old_path"];
                    // folder has to end with '/' if the user forgets to enter '/' (prevent folder conversion to file)
                    if (target_path.back() != '/') {
                        target_path += "/";
                    }

                    // check duplicates
                    vector<string> file_list = split(file_list_old, ',');
                    set<string> unique_files(file_list.begin(), file_list.end());
                    if (unique_files.find(target_path) != unique_files.end()) {
                        content = "Failed movied folder: folder existed!";
                        break;
                    }

                    // move all subfolders and files if contains the folder path as prefix
                    for (auto &path : paths) {
                        // extract the keys and move the values to the new path
                        if (path.find(old_path) == 0) {
                            size_t pos = path.find(":");
                            if (pos != string::npos) {                      // extra steps for subfiles
                                string old_file_path = path.substr(0, pos); // src/web/file2.c
                                string new_file_path = target_path + old_file_path.substr(old_path.length());
                                string keys = path.substr(pos + 1); // k1+k2+k3
                                vector<string> keys_list = split(keys, '+');
                                for (int i = 0; i < (int)keys_list.size(); i++) {
                                    string key = keys_list[i];
                                    string value = client->get(username + ":" + old_file_path, key);
                                    client->put(username + ":" + new_file_path, key, value);
                                    client->del(username + ":" + old_file_path, key);
                                }
                                cout << "old file path: " << old_file_path << endl;
                                cout << "new file path: " << new_file_path << endl;
                            }
                            path.replace(0, old_path.length(), target_path);
                        }
                    }
                    file_list_new = join(paths, ",");
                    cout << "file_list_new: " << file_list_new << endl;
                    client->cput(username, "storage", file_list_old, file_list_new);
                    cout << "CPUT recipient=" << username << endl;
                } catch (KVNoMatch &err) {
                    cout << "CPUT RPC race conditions: " << err.what() << endl;
                    continue;
                } catch (KVError &err) {
                    cout << "CPUT RPC failed: " << err.what() << endl;
                }
                break;
            }

        } else if (action == "movefile") {
            // example: old path: src/file1.c, target path: src/web3/file2.c
            content = "Successfully moved file";
            cout << params["old_path"] << " to " << params["target_path"] << endl;
            while (1) {
                try {
                    // string file_list_old = "src/file1.c:k1+k2,src/web/file2.c:k1+k2+k3,file3.c:k1";
                    string file_list_old = client->getdefault(username, "storage", "");
                    string file_list_new = file_list_old;
                    string target_path = params["target_path"];
                    string file_to_move = params["old_path"];
                    // files cannot end with '/' if the user enters '/' (prevent file conversion to folder)
                    if (target_path.back() == '/') {
                        target_path.pop_back();
                    }

                    // check duplicates
                    vector<string> paths = split(file_list_new, ',');
                    bool duplicate = false;
                    for (auto &path : paths) {
                        if (path.find(target_path) == 0) {
                            content = "Failed moved file: file existed!";
                            duplicate = true;
                            break;
                        }
                    }
                    if (duplicate) {
                        break;
                    }

                    for (auto &path : paths) {
                        // only files have ":" in the path
                        size_t pos = path.find(":");
                        if (pos != string::npos) {
                            string old_file_path = path.substr(0, pos); // src/web/file2.c
                            // found the file to move
                            if (old_file_path == file_to_move) {
                                string keys = path.substr(pos + 1); // k1+k2+k3
                                vector<string> keys_list = split(keys, '+');
                                for (int i = 0; i < (int)keys_list.size(); i++) {
                                    string key = keys_list[i];
                                    string value = client->get(username + ":" + old_file_path, key);
                                    client->put(username + ":" + target_path, key, value);
                                    client->del(username + ":" + old_file_path, key);
                                }
                                cout << "old file path: " << file_to_move << endl;
                                cout << "new file path: " << target_path << endl;
                                path.replace(0, file_to_move.length(), target_path);
                                break;
                            }
                        }
                    }
                    file_list_new = join(paths, ",");
                    cout << "file_list_new: " << file_list_new << endl;
                    client->cput(username, "storage", file_list_old, file_list_new);
                    cout << "CPUT recipient=" << username << endl;
                } catch (KVNoMatch &err) {
                    cout << "CPUT RPC race conditions: " << err.what() << endl;
                    continue;
                } catch (KVError &err) {
                    cout << "CPUT RPC failed: " << err.what() << endl;
                }
                break;
            }

        } else if (action == "uploadfile") {
            content = "Successfully uploaded file";
            string filename = "untitled";
            string new_file_path = filename + ":";
            try {
                // check if the file already exists
                string file_list_old = client->getdefault(username, "storage", "");
                vector<string> file_list = split(file_list_old, ',');
                set<string> unique_files(file_list.begin(), file_list.end());
                if (unique_files.find(filename) != unique_files.end()) {
                    content = "Failed uploading file: file existed!";
                } else {
                    for (int i = 0; i < (int)body.size(); i += 2000000) {
                        // update the keys
                        new_file_path += "k" + to_string(i) + "+";
                        // store the binary segments
                        if (i*2000000 + 2000000 < (int)body.size()) {
                            client->put(username + ":" + filename, "k" + to_string(i), body.substr(i, 2000000));
                        } else {
                            client->put(username + ":" + filename, "k" + to_string(i), body.substr(i));
                        }
                    }
                    if (new_file_path.back() == '+') {
                        new_file_path.pop_back();
                    }

                    // update the path list
                    string file_list_new;
                    if (file_list_old == "") {
                        file_list_new = new_file_path;
                    } else {
                        file_list_new = file_list_old + "," + new_file_path;
                    }
                    client->cput(username, "storage", file_list_old, file_list_new);
                }
            } catch (KVError &err) {
                cout << err.what() << endl;
            }

        } else if (action == "rename") {
            content = "Successfully renamed file";
            while (1) {
                try {
                    string file_list_old = client->getdefault(username, "storage", "");
                    string file_list_new = file_list_old;
                    string old_path;
                    string target_path;
                    vector<string> file_list;
                    // rename the folder
                    if (params["type"] == "folder") {
                        old_path = params["old_path"] + "/";
                        string parent_path = old_path.substr(0, params["old_path"].find_last_of("/\\") + 1);
                        cout << "parent_path: " << parent_path << endl;
                        if (params["new_name"].back() != '/') {
                            params["new_name"] += "/";
                        }
                        target_path = parent_path + params["new_name"];

                        // check duplicates
                        file_list = split(file_list_old, ',');
                        set<string> unique_files(file_list.begin(), file_list.end());
                        if (unique_files.find(target_path) != unique_files.end()) {
                            content = "Failed movied folder: folder existed!";
                            break;
                        }

                        // move all subfolders and files if contains the folder path as prefix
                        for (auto &path : file_list) {
                            // extract the keys and move the values to the new path
                            if (path.find(old_path) == 0) {
                                size_t pos = path.find(":");
                                if (pos != string::npos) {                      // extra steps for subfiles
                                    string old_file_path = path.substr(0, pos); // src/web/file2.c
                                    string new_file_path = target_path + old_file_path.substr(old_path.length());
                                    string keys = path.substr(pos + 1); // k1+k2+k3
                                    vector<string> keys_list = split(keys, '+');
                                    for (int i = 0; i < (int)keys_list.size(); i++) {
                                        string key = keys_list[i];
                                        string value = client->get(username + ":" + old_file_path, key);
                                        client->put(username + ":" + new_file_path, key, value);
                                        client->del(username + ":" + old_file_path, key);
                                    }
                                    cout << "old file path: " << old_file_path << endl;
                                    cout << "new file path: " << new_file_path << endl;
                                }
                                path.replace(0, old_path.length(), target_path);
                            }
                        }
                    } else { // rename the file
                        old_path = params["old_path"];
                        string parent_path = old_path.substr(0, params["old_path"].find_last_of("/\\") + 1);
                        // Extract the ":k1+k2+k3" part from the old file name
                        size_t colon_pos = old_path.find(":");
                        string suffix = (colon_pos != string::npos) ? old_path.substr(colon_pos) : "";
                        if (params["new_name"].back() == '/') {
                            params["new_name"].pop_back();
                        }
                        target_path = parent_path + params["new_name"] + suffix;

                        // check duplicates
                        file_list = split(file_list_old, ',');
                        set<string> unique_files(file_list.begin(), file_list.end());
                        if (unique_files.find(target_path) != unique_files.end()) {
                            content = "Failed movied folder: folder existed!";
                            break;
                        }

                        for (auto &path : file_list) {
                            // only files have ":" in the path
                            size_t pos = path.find(":");
                            if (pos != string::npos) {
                                string old_file_path = path.substr(0, pos); // src/web/file2.c
                                // found the file to move
                                if (old_file_path == old_path) {
                                    string keys = path.substr(pos + 1); // k1+k2+k3
                                    vector<string> keys_list = split(keys, '+');
                                    for (int i = 0; i < (int)keys_list.size(); i++) {
                                        string key = keys_list[i];
                                        string value = client->get(username + ":" + old_file_path, key);
                                        client->put(username + ":" + target_path, key, value);
                                        client->del(username + ":" + old_file_path, key);
                                    }
                                    cout << "old file path: " << old_path << endl;
                                    cout << "new file path: " << target_path << endl;
                                    path.replace(0, old_path.length(), target_path);
                                    break;
                                }
                            }
                        }
                    }

                    file_list_new = join(file_list, ",");
                    cout << "file_list_new: " << file_list_new << endl;
                    client->cput(username, "storage", file_list_old, file_list_new);
                    cout << "CPUT recipient=" << username << endl;
                } catch (KVNoMatch &err) {
                    cout << "CPUT RPC race conditions: " << err.what() << endl;
                    continue;
                } catch (KVError &err) {
                    cout << "CPUT RPC failed: " << err.what() << endl;
                }
                break;
            }
        }
    } else { // session is not valid
        content = "401 Unauthorized";
    }
    responseHeaders.insert(make_pair("Access-Control-Allow-Origin", "*"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Methods", "POST, GET, OPTIONS"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Headers", "Content-Type"));
    responseHeaders.insert(make_pair("Content-Type", "text/plain"));
    responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));
    return generate_response("200 OK", responseHeaders, content);
}

string RequestHandler::handle_delete_storage(const string &raw_request, string url, bool is_folder, unordered_map<string, string> &headers) {
    // parse the request body
    string body = "";
    size_t pos = raw_request.find("\r\n\r\n");
    if (pos == string::npos) {
        cout << "No message body found" << endl;
    } else {
        body = raw_request.substr(pos + 4);
        cout << "Message body: " << body << "\n";
    }
    string path_to_remove = body;

    unordered_map<string, string> responseHeaders;
    string content;
    string sid = get_cookie_from_header(headers);
    if (session_manager_->session_check(sid)) { // session is valid
        unordered_map<string, string> session_data = session_manager_->get_session(sid);
        string username = session_data["username"];
        if (is_folder) { // delete folder and all its subfolders and files
            content = "Successfully deleted folder";
            while (1) {
                try {
                    string file_list_old = client->getdefault(username, "storage", "");
                    string file_list_new = file_list_old;
                    vector<string> paths = split(file_list_new, ',');
                    // if contains the folder path as prefix, remove it
                    for (auto &path : paths) {
                        if (path.find(path_to_remove) == 0) {
                            // extract the keys
                            size_t pos = path.find(":");
                            if (pos != string::npos) {
                                string keys = path.substr(pos + 1);
                                vector<string> keys_list = split(keys, '+');
                                for (int i = 0; i < (int)keys_list.size(); i++) {
                                    string key = keys_list[i];
                                    // delete the key from kv
                                    client->del(username + ":" + path, key);
                                }
                            }
                            path = "";
                        }
                    }
                    // update the path list
                    file_list_new = join(paths, ",");
                    string doublecomma = ",,";
                    size_t pos = file_list_new.find(doublecomma);
                    if (pos != string::npos) {
                        file_list_new.replace(pos, doublecomma.length(), ",");
                    }
                    if (file_list_new.front() == ',') {
                        file_list_new = file_list_new.substr(1);
                    }
                    if (file_list_new.back() == ',') {
                        file_list_new.pop_back();
                    }
                    cout << "file_list_new: " << file_list_new << endl;
                    client->cput(username, "storage", file_list_old, file_list_new);
                    cout << "CPUT recipient=" << username << endl;
                } catch (KVNoMatch &err) {
                    cout << "CPUT RPC race conditions: " << err.what() << endl;
                    continue;
                } catch (KVError &err) {
                    cout << "CPUT RPC failed: " << err.what() << endl;
                }
                break;
            }
        } else { // delete file
            while (1) {
                try {
                    content = "Successfully deleted file";
                    string file_list_old = client->getdefault(username, "storage", "");
                    string file_list_new = file_list_old;
                    vector<string> paths = split(file_list_new, ',');
                    for (auto &path : paths) {
                        if (path.back() != '/' && path.find(path_to_remove) == 0) {
                            // extract the keys and remove them from kv
                            size_t colon_pos = path.find(":");
                            if (colon_pos != string::npos) {
                                string filepath = path.substr(0, colon_pos);
                                string keys = path.substr(colon_pos + 1);
                                vector<string> keys_list = split(keys, '+');
                                for (int i = 0; i < (int)keys_list.size(); i++) {
                                    string key = keys_list[i];
                                    client->del(username + ":" + filepath, key);
                                }
                            }
                            // remove the path from the path list
                            size_t pos = path.find_last_of("/");
                            if (colon_pos != string::npos) {
                                path = path.substr(0, pos + 1);
                            } else {
                                path = "";
                            }
                        }
                    }
                    file_list_new = join(paths, ",");
                    string doublecomma = ",,";
                    pos = file_list_new.find(doublecomma);
                    if (pos != string::npos) {
                        file_list_new.replace(pos, doublecomma.length(), ",");
                    }
                    if (file_list_new.front() == ',') {
                        file_list_new = file_list_new.substr(1);
                    }
                    if (file_list_new.back() == ',') {
                        file_list_new.pop_back();
                    }
                    client->cput(username, "storage", file_list_old, file_list_new);
                    cout << "CPUT recipient=" << username << endl;
                    cout << "file_list_new: " << file_list_new << endl;
                } catch (KVNoMatch &err) {
                    cout << "CPUT RPC race conditions: " << err.what() << endl;
                    continue;
                } catch (KVError &err) {
                    cout << "CPUT RPC failed: " << err.what() << endl;
                }
                break;
            }
        }
    } else { // session is not valid
        content = "401 Unauthorized";
    }
    responseHeaders.insert(make_pair("Access-Control-Allow-Origin", "*"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Methods", "POST, GET, OPTIONS"));
    responseHeaders.insert(make_pair("Access-Control-Allow-Headers", "Content-Type"));
    responseHeaders.insert(make_pair("Content-Type", "text/plain"));
    responseHeaders.insert(make_pair("Content-Length", to_string(content.size())));
    return generate_response("200 OK", responseHeaders, content);
}

string RequestHandler::serve_webpage(const string &location) {
    // Read the contents of the HTML file
    ifstream file(location);
    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    // Set the response headers
    unordered_map<string, string> headers;
    headers["Content-Type"] = "text/html";
    headers["Content-Length"] = to_string(content.length());

    // Generate the response
    string response = generate_response("200 OK", headers, content);

    return response;
}

// change (jinwei)
// Given a header map, return the value of the session ID from the Set-Cookie header if it exists
string RequestHandler::get_cookie_from_header(const unordered_map<string, string> &headers) {
    auto it = headers.find("Cookie");
    if (it != headers.end()) {
        // Extract the session ID from the cookie header value
        string cookie_value = it->second;
        cerr << "get Cookie value:" << cookie_value;
        size_t pos = cookie_value.find("session_id=");
        if (pos != string::npos) {
            pos += string("session_id=").length();
            cerr << "get session_id :" << cookie_value.substr(pos);
            //            size_t end_pos = cookie_value.find(";", pos);
            //            if (end_pos == string::npos) {
            //                end_pos = cookie_value.length();
            //            }
            //            return cookie_value.substr(pos, end_pos - pos);
            return cookie_value.substr(pos);
        }
    }
    return "";
}

string RequestHandler::serve_image(const string &location) {
    // Open the image file and read its contents
    std::ifstream image_file(location, std::ios::binary);
    std::ostringstream image_data;
    image_data << image_file.rdbuf();

    std::string content = image_data.str();

    // Set the response headers
    unordered_map<string, string> headers;
    headers["Content-Type"] = "image/png";
    headers["Content-Length"] = to_string(content.size());

    // Generate the response
    string response = generate_response("200 OK", headers, content);

    return response;
}

unordered_map<string, string> RequestHandler::parse_json(const string &body) {
    unordered_map<string, string> params;
    string key, value;
    bool isKey = true;
    bool isValue = false;
    bool inQuotes = false;

    for (char c : body) {
        if (c == '{' || c == '}' || c == '\t' || c == '\n') {
            continue;
        } else if (c == ':') {
            if (inQuotes) {
                if (isValue) {
                    value.push_back(c);
                }
            } else {
                isKey = false;
                isValue = true;
            }
        } else if (c == ',') {
            if (inQuotes) {
                if (isValue) {
                    value.push_back(c);
                }
            } else {
                params[key] = value;
                key.clear();
                value.clear();
                isKey = true;
                isValue = false;
            }
        } else if (c == '\"') {
            inQuotes = !inQuotes;
        } else if (inQuotes) {
            if (isKey) {
                key.push_back(c);
            } else if (isValue) {
                value.push_back(c);
            }
        }
    }

    if (!key.empty() && !value.empty()) {
        params[key] = value;
    }

    return params;
}

// string RequestHandler::generate_redirect(const string& location) {
//     string content = "";
//     unordered_map<string, string> headers;
//     headers["Content-Type"] = "text/html";
//     headers["Content-Length"] = to_string(content.length());
//     headers.insert(make_pair("Location", location));
//
//     return generate_response("302 Found", headers, content);
// }
