load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//bazel:defs.bzl", "LINKOPTS")

package(default_visibility = ["//visibility:public"])

cc_library(
   name = "webmail_utils",
   srcs = ["webmail_utils.cc"],
   hdrs = ["webmail_utils.h"],
)


cc_binary(
    name = "webmail",
    srcs = ["webmail.cc"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-client",
        "//src:kv-exceptions",
        "//src:utils",
       ":webmail_utils",
    ],
    linkopts = LINKOPTS + ["-lpthread -lresolv"],
)
