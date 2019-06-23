cc_binary(
  name = "main",
  srcs = [
      "main.cc",
  ],
  deps = [
      "@com_github_gflags_gflags//:gflags",
      "@com_github_google_glog//:glog",
      ":main_lib",
  ],
)

cc_library(
  name = "main_lib",
  hdrs = [
      "geometry.h",
      "intersection.h",
      "mine.h",
  ],
)
