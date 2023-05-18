load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//bazel:defs.bzl", "LINKOPTS")

package(default_visibility = ["//visibility:public"])

# loadbalancer
cc_binary(
    name = "loadbalancer",
    srcs = ["loadbalancer.cc"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
    ],
    linkopts = LINKOPTS,
)
