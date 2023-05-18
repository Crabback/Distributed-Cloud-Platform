load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//bazel:defs.bzl", "LINKOPTS")

cc_binary(
    name = "login_test",
    srcs = ["login_test.cc"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-client",
        "//src:kv-exceptions",
        "//src:utils",
    ],
    linkopts = LINKOPTS,
)
