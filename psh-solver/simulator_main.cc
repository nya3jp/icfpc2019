#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "simulator.h"

namespace {

std::string ReadFile(const std::string path) {
  std::ifstream fin(path);
  CHECK(fin);

  return std::string(
    std::istreambuf_iterator<char>(fin),
    std::istreambuf_iterator<char>());
}

} // namespace

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " [prob-XXX.desc]" << std::endl;
    return 1;
  }

  std::string desc_content = ReadFile(argv[1]);
  auto desc = icfpc2019::ParseDesc(desc_content);
  auto map = icfpc2019::Map(desc);

  LOG(INFO) << "remaining: " << map.remaining() << "\n" << map.ToString();
  map.Run(0, icfpc2019::Instruction{icfpc2019::Instruction::Type::D});
  LOG(INFO) << "remaining: " << map.remaining() << "\n" << map.ToString();
  return 0;
}
