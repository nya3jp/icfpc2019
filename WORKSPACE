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
