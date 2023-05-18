#pragma once

#include <exception>
#include <string>
#include <grpcpp/grpcpp.h>

using namespace grpc;
using namespace std;

string grpc_status_to_string(StatusCode code);

class KVError : public exception {
    StatusCode code;
    string msg, msg_with_code;
public:
    explicit KVError(const Status &status) : KVError(status.error_code(), status.error_message()) {}

    KVError(StatusCode code, const string &msg) : code(code), msg(msg) {
        msg_with_code = grpc_status_to_string(code) + ": " + msg;
    }

    const char *what() const noexcept override {
        return msg_with_code.c_str();
    }
};

// empty subclasses, used for nicer exception catching for predictable errors
class KVNotFound : public KVError {
    using KVError::KVError;
};

class KVNoMatch : public KVError {
    using KVError::KVError;
};