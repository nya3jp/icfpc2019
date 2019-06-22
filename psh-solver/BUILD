cc_binary(
  name = "sim",
  srcs = [
      "main.cc",
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
      "simulator.h",
  ],
  srcs = [
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
