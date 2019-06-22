cc_binary(
  name = "sim",
  srcs = [
      "simulator_main.cc",
  ],
  deps = [
      "@com_github_gflags_gflags//:gflags",
      "@com_github_google_glog//:glog",
      "@com_google_absl//absl/types:optional",
      ":simulator",
  ],
)

cc_library(
  name = "simulator",
  hdrs = [
      "rational.h",
      "simulator.h",
  ],
  srcs = [
      "rational.cc",
      "simulator.cc",
  ],
  deps = [
      "@com_github_gflags_gflags//:gflags",
      "@com_github_google_glog//:glog",
      "@com_google_absl//absl/types:optional",
      "@com_google_absl//absl/strings",
  ],
  visibility = ["//visibility:public"],
)
