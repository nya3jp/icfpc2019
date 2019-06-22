#include <iostream>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "psh-solver/simulator.h"

using namespace std;

namespace icfpc2019 {
namespace yuizumi {

using Program = vector<Instruction>;

class Solver {
public:
    explicit Solver(const Desc& desc)
        : map_(desc),
          done_(map_.height(), vector<uint8_t>(map_.width())) {}

    Program Solve() {
        SolveImpl();
        return program_;
    }

private:
    const Wrapper& wrapper() const {
        return map_.wrappers().front();
    }

    bool done(int x, int y) const {
        if (x < 0 || x >= map_.width() || y < 0 || y >= map_.height())
            return true;
        return done_[y][x] || map_[{x, y}] == Cell::WALL;
    }

    void Invoke(Instruction::Type type) {
        const Instruction inst = {type, {}};
        program_.push_back(inst);
        map_.Run(0, inst);
    }

    bool SolveImpl();

    Map map_;
    vector<vector<uint8_t>> done_;
    Program program_;
};

bool Solver::SolveImpl()
{
    if (map_.remaining() == 0)
        return true;

    const int x = wrapper().point().x;
    const int y = wrapper().point().y;
    done_[y][x] = true;

    if (!done(x - 1, y)) {
        Invoke(Instruction::Type::A);
        if (SolveImpl()) return true;
        Invoke(Instruction::Type::D);
    }
    if (!done(x + 1, y)) {
        Invoke(Instruction::Type::D);
        if (SolveImpl()) return true;
        Invoke(Instruction::Type::A);
    }
    if (!done(x, y - 1)) {
        Invoke(Instruction::Type::S);
        if (SolveImpl()) return true;
        Invoke(Instruction::Type::W);
    }
    if (!done(x, y + 1)) {
        Invoke(Instruction::Type::W);
        if (SolveImpl()) return true;
        Invoke(Instruction::Type::S);
    }

    return false;
}

Program Solve(const string& s) {
    Solver solver(ParseDesc(s));
    return solver.Solve();
}

}  // namespace yuizumi
}  // namespace icfpc2019

int main()
{
    string s;
    cin >> s;
    for (const auto& inst : icfpc2019::yuizumi::Solve(s))
        cout << inst;
    cout << endl;
    return 0;
}
