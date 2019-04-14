# -*- mode: python -*-

# Workspace documentation:
# https://docs.bazel.build/versions/master/be/workspace.html

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Abseil (https://abseil.io/)
#
# Mainly for string functions.
# https://abseil.io/docs/cpp/guides/strings
# https://abseil.io/docs/cpp/guides/format
http_archive(
    name = "com_google_absl",
    sha256 = "3eedb0f05e118f29e501dfb1046940eeadee326081aaeb8fc737f2eef2129ace",
    strip_prefix = "abseil-cpp-a02f62f456f2c4a7ecf2be3104fe0c6e16fbad9a",
    urls = ["https://github.com/abseil/abseil-cpp/archive/a02f62f456f2c4a7ecf2be3104fe0c6e16fbad9a.zip"],  # 2019-04-12
)

# gflags (https://gflags.github.io/gflags/)
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "19713a36c9f32b33df59d1c79b4958434cb005b5b47dc5400a7a4b078111d9b5",
    strip_prefix = "gflags-2.2.2",
    urls = ["https://github.com/gflags/gflags/archive/v2.2.2.zip"],
)

# glog (https://github.com/google/glog)
http_archive(
    name = "com_github_google_glog",
    sha256 = "e94a39c4ac6fab6fdf75b37201e0333dce7fbd996e3f9c4337136ea2ecb634fc",
    strip_prefix = "glog-6ca3d3cf5878020ebed7239139d6cd229a1e7edb",
    urls = ["https://github.com/google/glog/archive/6ca3d3cf5878020ebed7239139d6cd229a1e7edb.zip"],  # 2019-04-12
)

# gtest (https://github.com/google/googletest)
http_archive(
    name = "com_github_google_googletest",
    sha256 = "927827c183d01734cc5cfef85e0ff3f5a92ffe6188e0d18e909c5efebf28a0c7",
    strip_prefix = "googletest-release-1.8.1",
    urls = ["https://github.com/google/googletest/archive/release-1.8.1.zip"],
)

# Protocol Buffers (https://github.com/protocolbuffers/protobuf/)
http_archive(
    name = "com_google_protobuf",
    sha256 = "f976a4cd3f1699b6d20c1e944ca1de6754777918320c719742e1674fcf247b7e",
    strip_prefix = "protobuf-3.7.1",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.7.1.zip"],
)

# zlib required for Protocol Buffers
http_archive(
    name = "net_zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
    strip_prefix = "zlib-1.2.11",
    urls = ["https://zlib.net/zlib-1.2.11.tar.gz"],
)
bind(
    name = "zlib",
    actual = "@net_zlib//:zlib",
)

# Bazel Skylib (https://github.com/bazelbuild/bazel-skylib)
http_archive(
    name = "bazel_skylib",
    sha256 = "2ef429f5d7ce7111263289644d233707dba35e39696377ebab8b0bc701f7818e",
    type = "tar.gz",
    urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/0.8.0/bazel-skylib.0.8.0.tar.gz"],
)
