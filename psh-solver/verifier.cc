#include "simulator.h"

#include <fstream>
#include <iterator>
#include <iostream>
#include "glog/logging.h"

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
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " [prob-XXX.desc] [prob-XXX.sol]";
    return 2;
  }

  auto desc_content = ReadFile(argv[1]);
  auto sol_content = ReadFile(argv[2]);

  auto desc = icfpc2019::ParseDesc(desc_content);
  auto map = icfpc2019::Map(desc);
  auto sol = icfpc2019::ParseSolution(sol_content);

  return !icfpc2019::Verify(&map, sol);
}
