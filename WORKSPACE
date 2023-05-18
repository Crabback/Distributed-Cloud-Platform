load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

# ==== gRPC ===
http_archive(
    name = "com_github_grpc_grpc",
    urls = [
        "https://github.com/grpc/grpc/archive/358bfb581feeda5bf17dd3b96da1074d84a6ef8d.tar.gz",
    ],
    strip_prefix = "grpc-358bfb581feeda5bf17dd3b96da1074d84a6ef8d",
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()
load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
grpc_extra_deps()

# ==== openSSL ====
# (for hashes)
new_local_repository(
    name = "openssl",
    path = "/usr/include/openssl",
    build_file_content = """
package(default_visibility = ["//visibility:public"])
cc_library(
    name = "headers",
    hdrs = glob(["**/*.h"]),
    includes = ["."],
)
"""
)

# ==== {fmt} ====
# since not all compilers support c++20 format() yet
git_repository(
    name = "fmt",
    branch = "master",
    remote = "https://github.com/fmtlib/fmt",
    patch_cmds = [
        "mv support/bazel/.bazelrc .bazelrc",
        "mv support/bazel/.bazelversion .bazelversion",
        "mv support/bazel/BUILD.bazel BUILD.bazel",
        "mv support/bazel/WORKSPACE.bazel WORKSPACE.bazel",
    ],
    # Windows-related patch commands are only needed in the case MSYS2 is not installed.
    # More details about the installation process of MSYS2 on Windows systems can be found here:
    # https://docs.bazel.build/versions/main/install-windows.html#installing-compilers-and-language-runtimes
    # Even if MSYS2 is installed the Windows related patch commands can still be used.
    patch_cmds_win = [
        "Move-Item -Path support/bazel/.bazelrc -Destination .bazelrc",
        "Move-Item -Path support/bazel/.bazelversion -Destination .bazelversion",
        "Move-Item -Path support/bazel/BUILD.bazel -Destination BUILD.bazel",
        "Move-Item -Path support/bazel/WORKSPACE.bazel -Destination WORKSPACE.bazel",
    ],
)

# ==== json ====
http_archive(
    name = "com_github_nlohmann_json",
    build_file = "//bazel:json.BUILD", # see below
    strip_prefix = "json-3.11.2",
    urls = ["https://github.com/nlohmann/json/archive/v3.11.2.tar.gz"],
    sha256 = "d69f9deb6a75e2580465c6c4c5111b89c4dc2fa94e3a85fcd2ffcd9a143d9273",
)

# curl repository
#http_archive(
#    name = "curl",
#    urls = ["https://github.com/curl/curl/archive/refs/tags/curl-7_78_0.tar.gz"],
#    sha256 = "your_sha256_checksum_here",
#    strip_prefix = "curl-curl-7_78_0",
#)
