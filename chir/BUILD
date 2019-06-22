cc_binary(
  name = "simple",
  srcs = [
      "simpla_main.cc",
  ],
  deps = [
      "@com_github_gflags_gflags//:gflags",
      "@com_github_google_glog//:glog",
      ":simple_lib",
  ],
)

cc_library(
  name = "simple_lib",
  hdrs = [
      "geometry.h",
      "intersection.h",
      "mine.h",
      "string.h",
  ],
  srcs = [
      "string.cc",
  ],
)
