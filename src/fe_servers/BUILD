load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//bazel:defs.bzl", "LINKOPTS")

#request_handler
cc_library(
    name = "request_handler",
    srcs = ["request_handler.cc"],
    hdrs = ["request_handler.h", "//src/fe_servers:session_manager.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-client",
        "//src:kv-exceptions",
        "//src:utils",
        "//src/webmail:webmail_utils",
    ],
)

#admin_request_handler
#cc_library(
#    name = "admin_request_handler",
#    srcs = ["admin_request_handler.cc"],
#    hdrs = ["admin_request_handler.h"],
#    deps = [
#        "@com_github_grpc_grpc//:grpc++",
#        "//src:kv-master-client",
#        "//src:kv-exceptions",
#        "//src:utils",
#        "//src/fe_servers:admin_console",
#    ],
#)

# admin_console
cc_library(
   name = "admin_console",
   srcs = ["admin_console.cc"],
   hdrs = ["admin_console.h"],
   deps = [
       "@com_github_grpc_grpc//:grpc++",
       "//src:kv-master-client",
       "//src:kv-exceptions",
       "//src:utils",
   ],
)

#curl
#cc_library(
#    name = "curl",
#    srcs = glob(["@curl//:lib/*.c"]),
#    hdrs = glob(["@curl//:include/curl/*.h"]),
#    includes = ["@curl//:include"],
#    visibility = ["//visibility:public"],
#)

# fe_server
cc_binary(
    name = "frontend_server",
    srcs = ["frontend_server.cc"],
    deps = [
    	"//src/fe_servers:request_handler",
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-client",
        "//src:kv-exceptions",
        "//src:utils",
    ],
    linkopts = LINKOPTS + ["-lpthread -lresolv"],
)

#admin_server
cc_binary(
    name = "admin_server",
    srcs = ["admin_server.cc"],
    deps = [
    	"//src/fe_servers:admin_console",
    	"//src:kv-client",
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-exceptions",
        "//src:utils",
        "@com_github_nlohmann_json//:json",
    ],
    linkopts = LINKOPTS + ["-lpthread -lresolv"],
)
