load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("//bazel:defs.bzl", "LINKOPTS")

package(default_visibility = ["//visibility:public"])

# protos
cc_grpc_library(
    name = "kv_grpc",
    srcs = ["protos/kv.proto"],
    deps = []
)

cc_grpc_library(
    name = "kvmaster_grpc",
    srcs = ["protos/kvmaster.proto"],
    deps = []
)

# common
cc_library(
    name = "utils",
    srcs = ["common/utils.cc"],
    hdrs = ["common/utils.h"],
)

# kv
cc_library(
    name = "kv-client",
    srcs = ["kv/client.cc"],
    hdrs = ["kv/client.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@openssl//:headers",
        ":kv_grpc",
        ":kvmaster_grpc",
        ":kv-exceptions",
        ":utils",
    ],
    linkopts = ["-L/usr/local/opt/openssl/lib -lssl -lcrypto"],
)

cc_library(
    name = "kv-master-client",
    srcs = ["kv/master_client.cc"],
    hdrs = ["kv/master_client.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        ":kv_grpc",
        ":kv-exceptions",
        ":utils",
    ],
)

cc_library(
    name = "kv-exceptions",
    hdrs = ["kv/exceptions.h"],
    srcs = ["kv/exceptions.cc"],
    deps = ["@com_github_grpc_grpc//:grpc++"],
)

# executables
cc_binary(
    name = "kv",
    srcs = ["kv.cc"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@fmt",
        ":kv_grpc",
        ":kvmaster_grpc",
        ":kv-exceptions",
        ":utils",
    ],
    linkopts = LINKOPTS,
)

cc_binary(
    name = "kvmaster",
    srcs = ["kvmaster.cc"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        ":kvmaster_grpc",
        ":kv-master-client",
        ":utils",
    ],
    linkopts = LINKOPTS,
)
