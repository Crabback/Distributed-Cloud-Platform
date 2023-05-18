#include <unordered_map>
#include <string>
#include <random>
#include <iostream>

using namespace std;

class SessionManager {
public:
    string create_session(const string& username) {
        string session_id = generate_session_id();
        unordered_map<string, string> session_data;
        session_data["username"] = username;
        sessions_[session_id] = move(session_data);
        return session_id;
    }

    unordered_map<string, string> get_session(const string& session_id) {
        if (sessions_.find(session_id) != sessions_.end()) {
            return sessions_[session_id];
        }
        return {};
    }

    void update_session(const string& session_id, const unordered_map<string, string>& data) {
        sessions_[session_id] = data;
    }

    void delete_session(const string& session_id) {
        sessions_.erase(session_id);
    }

    string get_cookie_value(const string& session_id) {
        return "session_id=" + session_id + "; HttpOnly; Secure";
    }

    // Check if the request contains a valid session ID in the Set-Cookie header
    bool session_check(const string& session_id) {
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return false;
        }
        return true;
    }

private:
    unordered_map<string, unordered_map<string, string>> sessions_;
    random_device rd;
    mt19937 gen{rd()};
    uniform_int_distribution<> dis{0, 15};

    string generate_session_id() {
        string session_id;
        for (int i = 0; i < 32; i++) {
            session_id += hex_digit();
        }
        return session_id;
    }

    char hex_digit() {
        static const char* digits = "0123456789abcdef";
        return digits[dis(gen)];
    }
};
