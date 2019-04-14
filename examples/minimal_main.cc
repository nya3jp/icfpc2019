#include <iostream>

#include "examples/minimal_lib.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  std::cout << BuildHelloMessage() << std::endl;

  return 0;
}
