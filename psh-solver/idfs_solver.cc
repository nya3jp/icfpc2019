#include "simulator.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "gflags/gflags.h"
#include "glog/logging.h"

namespace {

std::string ReadContent(std::istream& is) {
  return std::string(
    std::istreambuf_iterator<char>(is),
    std::istreambuf_iterator<char>());
}

bool SolveInternal(icfpc2019::Map* m, int deps,
                   std::vector<icfpc2019::Instruction>* current) {
  if (m->remaining() == 0)
    return true;

  if (deps == 0)
    return false;

  const icfpc2019::Instruction cands[] = {
    {icfpc2019::Instruction::Type::W},
    {icfpc2019::Instruction::Type::A},
    {icfpc2019::Instruction::Type::S},
    {icfpc2019::Instruction::Type::D},
    {icfpc2019::Instruction::Type::Q},
    {icfpc2019::Instruction::Type::E},
  };

  for (const auto& cand : cands) {
    if (m->Run(0, cand) == icfpc2019::Map::RunResult::SUCCESS) {
      current->push_back(cand);
      if (SolveInternal(m, deps-1, current))
        return true;
      current->pop_back();
      m->Undo();
    }
  }

  return false;
}

void Solve(icfpc2019::Map* m) {
  for (int i = 1; i < 1000; ++i) {
    LOG(INFO) << "Trying " << i;
    std::vector<icfpc2019::Instruction> result;
    if (SolveInternal(m, i, &result)) {
      for (const auto& inst : result)
        std::cout << inst;
      std::cout << std::endl;
      return;
    }
  }
}

}  // namespace


int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  std::string desc_content;
  if (argc == 0) {
    desc_content = ReadContent(std::cin);
  } else {
    std::ifstream fin(argv[1]);
    desc_content = ReadContent(fin);
  }

  auto desc = icfpc2019::ParseDesc(desc_content);
  auto map = icfpc2019::Map(desc);

  Solve(&map);
}
