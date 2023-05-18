load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//bazel:defs.bzl", "LINKOPTS")

cc_binary(
    name = "kvtest",
    srcs = ["kvtest.cc"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-client",
        "//src:kv-exceptions",
        "//src:utils",
    ],
    linkopts = LINKOPTS,
)

cc_binary(
    name = "kvshell",
    srcs = ["kvshell.cc"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-client",
        "//src:kv-exceptions",
        "//src:utils",
    ],
    linkopts = LINKOPTS,
)
