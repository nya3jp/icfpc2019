# -*- mode: python -*-

# Rules documentation:
# https://docs.bazel.build/versions/master/be/overview.html#rules

cc_binary(
    name = "minimal_main",
    srcs = [
        "minimal_main.cc",
    ],
    deps = [
        ":minimal_lib",
        "@com_github_gflags_gflags//:gflags",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "minimal_lib",
    srcs = [
        "minimal_lib.cc",
    ],
    hdrs = [
        "minimal_lib.h",
    ],
    deps = [
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "minimal_lib_test",
    srcs = [
        "minimal_lib_test.cc",
    ],
    deps = [
        ":minimal_lib",
        "@com_github_google_googletest//:gtest",
        "@com_github_google_googletest//:gtest_main",
    ],
)
