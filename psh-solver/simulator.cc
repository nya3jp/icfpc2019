#include "simulator.h"

#include <algorithm>
#include <iostream>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "glog/logging.h"

#include "rational.h"

namespace icfpc2019 {
namespace {

Point ParsePoint(absl::string_view s) {
  int pos[2];
  int index = 0;
  for (auto token : absl::StrSplit(s, ',')) {
    CHECK(absl::SimpleAtoi(token, pos + (index++)));
  }
  return Point{pos[0], pos[1]};
}

std::vector<Point> ParseMap(absl::string_view s) {
  std::vector<Point> result;
  for (auto token : absl::StrSplit(s, ')', absl::SkipEmpty())) {
    int start = token[0] == ',' ? 2 : 1;
    token = token.substr(start, token.length() - start);
    result.push_back(ParsePoint(token));
  }
  return result;
}

std::vector<std::vector<Point>> ParseObstacles(absl::string_view s) {
  if (s.empty())
    return {};
  std::vector<std::vector<Point>> result;
  for (auto token : absl::StrSplit(s, ';')) {
    result.push_back(ParseMap(token));
  }
  return result;
}

std::vector<std::pair<Point, Booster>> ParseBoosters(absl::string_view s) {
  if (s.empty())
    return {};

  std::vector<std::pair<Point, Booster>> result;
  for (auto token : absl::StrSplit(s, ';')) {
    auto p = ParsePoint(token.substr(2, token.length() - 3));
    Booster b;
    switch (token[0]) {
      case 'B': b = Booster::B; break;
      case 'F': b = Booster::F; break;
      case 'L': b = Booster::L; break;
      case 'X': b = Booster::X; break;
      case 'R': b = Booster::R; break;
      case 'C': b = Booster::C; break;
      default:
        LOG(FATAL) << "Unknown booster: " << token;
    }
    result.emplace_back(p, b);
  }
  return result;
}

std::vector<Instruction> ParseInstructionList(absl::string_view s) {
  std::vector<Instruction> result;
  for (std::size_t i = 0; i < s.length(); ++i) {
    Instruction inst;
    switch (s[i]) {
      case 'W': inst.type = Instruction::Type::W; break;
      case 'S': inst.type = Instruction::Type::S; break;
      case 'A': inst.type = Instruction::Type::A; break;
      case 'D': inst.type = Instruction::Type::D; break;
      case 'Q': inst.type = Instruction::Type::Q; break;
      case 'E': inst.type = Instruction::Type::E; break;
      case 'Z': inst.type = Instruction::Type::Z; break;
      case 'B': inst.type = Instruction::Type::B; break;
      case 'F': inst.type = Instruction::Type::F; break;
      case 'L': inst.type = Instruction::Type::L; break;
      case 'R': inst.type = Instruction::Type::R; break;
      case 'T': inst.type = Instruction::Type::T; break;
      case 'C': inst.type = Instruction::Type::C; break;
    }
    if(s[i] == 'B' || s[i] == 'T'){
      int begin = i + 2;
      int end = s.find(')', begin);
      inst.arg = ParsePoint(s.substr(begin, end - begin));
      i = end;
    }
    result.push_back(std::move(inst));
  }
  return result;
}

std::vector<Cell> ConvertMap(
    std::size_t width,
    std::size_t height,
    const std::vector<Point>& m,
    const std::vector<std::vector<Point>>& obstacles) {
  std::vector<Cell> result(width * height, Cell::WALL);
  std::vector<int> bars;
  // map.
  for (int y = 0; y < static_cast<int>(height); ++y) {
    bars.clear();
    for (std::size_t i = 0; i < m.size(); ++i) {
      const auto& p1 = m[i];
      const auto& p2 = m[(i + 1) % m.size()];
      if (p1.x != p2.x) continue;
      auto yrange = std::minmax(p1.y, p2.y);
      if (yrange.first <= y && y < yrange.second) {
        bars.push_back(p1.x);
      }
    }
    std::sort(bars.begin(), bars.end());
    for (std::size_t i = 0; i < bars.size(); i += 2) {
      for (int x = bars[i]; x < bars[i + 1]; ++x) {
        result[y * width + x] = Cell::EMPTY;
      }
    }
  }

  // obstacles.
  for (int y = 0; y < static_cast<int>(height); ++y) {
    bars.clear();
    for (const auto& obstacle : obstacles) {
      for (std::size_t i = 0; i < obstacle.size(); ++i) {
        const auto& p1 = obstacle[i];
        const auto& p2 = obstacle[(i + 1) % obstacle.size()];
        if (p1.x != p2.x) continue;
        auto yrange = std::minmax(p1.y, p2.y);
        if (yrange.first <= y && y < yrange.second) {
          bars.push_back(p1.x);
        }
      }
    }
    std::sort(bars.begin(), bars.end());
    for (std::size_t i = 0; i < bars.size(); i += 2) {
      for (int x = bars[i]; x < bars[i + 1]; ++x) {
        result[y * width + x] = Cell::WALL;
      }
    }
  }

  return result;
}

bool IsVisible(const Point& origin, const Point& target,
               const std::vector<Cell>& m,
               std::size_t width, std::size_t height) {
  std::vector<Point> points;
  if (origin.x == target.x) {
    int miny, maxy;
    std::tie(miny, maxy) = std::minmax(origin.y, target.y);
    for (int y = miny; y <= maxy; ++y) {
      points.push_back(Point{origin.x, y});
    }
  } else {
    Point s = origin, g = target;
    if (s.x > g.x) {
      std::swap(s, g);
    }

    const auto grad = Rational(g.y - s.y, g.x - s.x);
    for (int x = s.x; x <= g.x; ++x) {
      auto left = std::max(Rational(s.x), Rational(x) - Rational(1, 2));
      auto right = std::min(Rational(g.x), Rational(x) + Rational(1, 2));

      auto left_y =
          Rational(s.y) + (left - Rational(s.x)) * grad + Rational(1, 2);
      auto right_y =
          Rational(s.y) + (right - Rational(s.x)) * grad + Rational(1, 2);
      int lo = std::min(Floor(left_y), Floor(right_y));
      int hi = std::max(Ceil(left_y), Ceil(right_y));
      for (int y = lo; y < hi; ++y) {
        points.push_back(Point{x, y});
      }
    }
  }

  for (const auto& p : points) {
    if (p.y < 0 || static_cast<int>(height) <= p.y ||
        p.x < 0 || static_cast<int>(width) <= p.x ||
        m[p.y * width + p.x] == Cell::WALL) {
      return false;
    }
  }
  return true;
}

bool IsPossibleToExtendManipualtor(
    const Wrapper& wrapper, const Point& p) {
  constexpr Point kDirs[] = {
    {0, 1}, {0, -1}, {1, 0}, {-1, 0},
  };
  // TODO check.
  if (p == Point{0, 0}) {
    return false;
  }
  for (const auto& manip : wrapper.manipulators()) {
    if (p == manip) {
      return false;
    }
  }

  for (const auto& dir : kDirs) {
    const auto cand = p + dir;
    if (cand == Point{0, 0})
      return true;
    for (const auto& manip : wrapper.manipulators()) {
      if (cand == manip)
        return true;
    }
  }
  return false;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const Point& point) {
  return os << "(" << point.x << "," << point.y << ")";
}

std::istream& operator>>(std::istream& is, Point& point) {
  char c;
  is >> c;
  CHECK(c == '(') << "Failed to find '(' when reading point from stream";
  if(!is) return is;
  int x, y;
  is >> x >> c >> y;
  CHECK(c == ',') << "Failed to find ',' when reading point from stream";
  if(!is) return is;
  is >> c;
  CHECK(c == ')') << "Failed to find ')' when reading point from stream";
  point.x = x;
  point.y = y;
  return is;
}

std::ostream& operator<<(std::ostream& os, const Instruction& inst) {
  os << inst.type;
  switch (inst.type) {
    case Instruction::Type::B:
    case Instruction::Type::T:
      os << inst.arg;
      break;
    default:
      // Do nothing.
      ;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const Instruction::Type& type) {
  switch (type) {
#define OUTPUT(x) \
    case Instruction::Type::x: os << #x; break

    OUTPUT(W);
    OUTPUT(S);
    OUTPUT(A);
    OUTPUT(D);
    OUTPUT(Q);
    OUTPUT(E);
    OUTPUT(Z);
    OUTPUT(B);
    OUTPUT(F);
    OUTPUT(L);
    OUTPUT(R);
    OUTPUT(T);
    OUTPUT(C);
#undef OUTPUT
    default:
      os << "Unknown(" << static_cast<int>(type) << ")";
  }
  return os;
}

std::istream& operator>>(std::istream& is, Instruction &inst)
{
  char c;
  is >> c;
  Instruction::Type t;
  switch(c) {
  case 'W': t = Instruction::Type::W; break;
  case 'S': t = Instruction::Type::S; break;
  case 'A': t = Instruction::Type::A; break;
  case 'D': t = Instruction::Type::D; break;
  case 'Q': t = Instruction::Type::Q; break;
  case 'E': t = Instruction::Type::E; break;
  case 'Z': t = Instruction::Type::Z; break;
  case 'B': t = Instruction::Type::B; break;
  case 'F': t = Instruction::Type::F; break;
  case 'L': t = Instruction::Type::L; break;
  case 'R': t = Instruction::Type::R; break;
  case 'T': t = Instruction::Type::T; break;
  case 'C': t = Instruction::Type::C; break;
  }
  inst.type = t;
  if(c == 'B' || c == 'T'){
    Point arg;
    is >> arg;
    inst.arg = arg;
  }
  return is;
}

Desc ParseDesc(const std::string& task) {
  Desc result;
  int index = 0;
  for (auto token : absl::StrSplit(task, '#')) {
    switch (index++) {
      case 0:
        result.map_ = ParseMap(token);
        break;
      case 1:
        result.point = ParsePoint(token.substr(1, token.length() - 2));
        break;
      case 2:
        result.obstacles = ParseObstacles(token);
        break;
      case 3:
        result.boosters = ParseBoosters(token);
        break;
      default:
        LOG(FATAL) << "Unkwnon task: " << task;
    }
  }

  return result;
}

Solution ParseSolution(const std::string& solution) {
  Solution result;
  for (auto token : absl::StrSplit(solution, '#')) {
    result.moves.emplace_back(ParseInstructionList(token));
  }
  return result;
}

Map::Map(const Desc& desc) {
  width_ = 0;
  height_ = 0;
  for (const auto& p : desc.map_) {
    width_ = std::max(static_cast<std::size_t>(p.x), width_);
    height_ = std::max(static_cast<std::size_t>(p.y), height_);
  }
  map_ = ConvertMap(width_, height_, desc.map_, desc.obstacles);
  booster_map_.insert(desc.boosters.begin(), desc.boosters.end());
  wrappers_.push_back(Wrapper(desc.point));
  Fill(wrappers_[0], nullptr);
  remaining_ = std::count(map_.begin(), map_.end(), Cell::EMPTY);
}

void Map::Undo() {
  const auto& log = backlogs_.back();
  auto& wrapper = wrappers_[log.wrapper_index()];

  switch (log.action()) {
    case BacklogEntry::Action::WW: {
      if (log.second_booster() != Booster::X) {
        booster_map_.emplace(wrapper.point(), log.second_booster());
      }
      const auto p = wrapper.point() - Point{0, 1};
      wrapper.set_point(p);
      // fallthrough.
    }
    case BacklogEntry::Action::W: {
      if (log.first_booster() != Booster::X) {
        booster_map_.emplace(wrapper.point(), log.first_booster());
      }
      const auto p = wrapper.point() - Point{0, 1};
      wrapper.set_point(p);
      break;
    }
    case BacklogEntry::Action::AA: {
      if (log.second_booster() != Booster::X) {
        booster_map_.emplace(wrapper.point(), log.second_booster());
      }
      const auto p = wrapper.point() - Point{-1, 0};
      wrapper.set_point(p);
      // fallthrough.
    }
    case BacklogEntry::Action::A: {
      if (log.first_booster() != Booster::X) {
        booster_map_.emplace(wrapper.point(), log.first_booster());
      }
      const auto p = wrapper.point() - Point{-1, 0};
      wrapper.set_point(p);
      break;
    }
    case BacklogEntry::Action::SS: {
      if (log.second_booster() != Booster::X) {
        booster_map_.emplace(wrapper.point(), log.second_booster());
      }
      const auto p = wrapper.point() - Point{0, -1};
      wrapper.set_point(p);
      // fallthrough.
    }
    case BacklogEntry::Action::S: {
      if (log.first_booster() != Booster::X) {
        booster_map_.emplace(wrapper.point(), log.first_booster());
      }
      const auto p = wrapper.point() - Point{0, -1};
      wrapper.set_point(p);
      break;
    }
    case BacklogEntry::Action::DD: {
      if (log.second_booster() != Booster::X) {
        booster_map_.emplace(wrapper.point(), log.second_booster());
      }
      const auto p = wrapper.point() - Point{0, -1};
      wrapper.set_point(p);
      // fallthrough.
    }
    case BacklogEntry::Action::D: {
      if (log.first_booster() != Booster::X) {
        booster_map_.emplace(wrapper.point(), log.first_booster());
      }
      const auto p = wrapper.point() - Point{1, 0};
      wrapper.set_point(p);
      break;
    }
    case BacklogEntry::Action::Z:
      break;
    case BacklogEntry::Action::Q: {
      wrapper.RotateClockwise();
      break;
    }
    case BacklogEntry::Action::E: {
      wrapper.RotateCounterClockwise();
      break;
    }
    case BacklogEntry::Action::B: {
      wrapper.RemoveManipulator();
      ++collected_b_;
      break;
    }
    case BacklogEntry::Action::F: {
      // Do nothing. Counter will be reset below.
      ++collected_f_;
      break;
    }
    case BacklogEntry::Action::L: {
      // Do nothing. Counter will be reset below.
      ++collected_l_;
      break;
    }
    case BacklogEntry::Action::R: {
      resets_.erase(wrapper.point());
      ++collected_r_;
      break;
    }
    case BacklogEntry::Action::T: {
      wrapper.set_point(log.orig_pos());
      break;
    }
    case BacklogEntry::Action::C: {
      wrappers_.pop_back();
      ++collected_c_;
      break;
    }
  }
  Unfill(log.updated_cells());

  switch (log.pending_booster()) {
    case Booster::B:
      --collected_b_;
      break;
    case Booster::F:
      --collected_f_;
      break;
    case Booster::L:
      --collected_l_;
      break;
    case Booster::R:
      --collected_r_;
      break;
    case Booster::C:
      --collected_c_;
      break;
    case Booster::X:
      // Do nothing.
      break;
  }
  wrapper.set_pending_booster(Booster::X);
  --num_steps_;
  backlogs_.pop_back();
}

void Map::Unfill(const std::vector<std::pair<Point, Cell>>& cells) {
  for (const auto& item : cells) {
    GetCell(item.first) = item.second;
    if (item.second == Cell::EMPTY)
      ++remaining_;
  }
}

void Map::Run(int index, const Instruction& inst) {
  ++num_steps_;
  BacklogEntry entry;
  entry.set_wrapper_index(index);
  // TODO record back log.
  CHECK_LT(index, static_cast<int>(wrappers_.size()));
  {
    auto& wrapper = wrappers_[index];
    entry.set_drill_count(wrapper.drill_count());
    entry.set_fast_count(wrapper.fast_count());

    entry.set_pending_booster(wrapper.pending_booster());
    switch (wrapper.pending_booster()) {
      case Booster::B:
        ++collected_b_;
        break;
      case Booster::F:
        ++collected_f_;
        break;
      case Booster::L:
        ++collected_l_;
        break;
      case Booster::R:
        ++collected_r_;
        break;
      case Booster::C:
        ++collected_c_;
        break;
      case Booster::X:
        // Do nothing.
        break;
    }
    wrapper.set_pending_booster(Booster::X);

    switch (inst.type) {
      case Instruction::Type::W:
        Move(&wrapper, Point{0, 1}, &entry,
             BacklogEntry::Action::W, BacklogEntry::Action::WW);
        break;
      case Instruction::Type::S:
        Move(&wrapper, Point{0, -1}, &entry,
             BacklogEntry::Action::S, BacklogEntry::Action::SS);
        break;
      case Instruction::Type::A:
        Move(&wrapper, Point{-1, 0}, &entry,
             BacklogEntry::Action::A, BacklogEntry::Action::AA);
        break;
      case Instruction::Type::D:
        Move(&wrapper, Point{1, 0}, &entry,
             BacklogEntry::Action::D, BacklogEntry::Action::DD);
        break;
      case Instruction::Type::Q:
        entry.set_action(BacklogEntry::Action::Q);
        wrapper.RotateCounterClockwise();
        Fill(wrapper, &entry);
        break;
      case Instruction::Type::E:
        entry.set_action(BacklogEntry::Action::E);
        wrapper.RotateClockwise();
        Fill(wrapper, &entry);
        break;
      case Instruction::Type::Z:
        entry.set_action(BacklogEntry::Action::Z);
        break;
      case Instruction::Type::B: {
        entry.set_action(BacklogEntry::Action::B);
        CHECK_GT(collected_b_, 0);
        CHECK(IsPossibleToExtendManipualtor(wrapper, inst.arg));
        wrapper.AddManipulator(inst.arg);
        --collected_b_;
        Fill(wrapper, &entry);
        break;
      }
      case Instruction::Type::F: {
        entry.set_action(BacklogEntry::Action::F);
        CHECK_GT(collected_f_, 0);
        wrapper.set_fast_count(51); // Including this turn.
        --collected_f_;
        break;
      }
      case Instruction::Type::L: {
        entry.set_action(BacklogEntry::Action::L);
        CHECK_GT(collected_l_, 0);
        wrapper.set_drill_count(31);  // Including this turn.
        --collected_l_;
        break;
      }
      case Instruction::Type::R: {
        entry.set_action(BacklogEntry::Action::R);
        // Note: the reset position created by Wrapper-i will be immediately
        // appeared to the Wrapper-j where i < j.
        CHECK_GT(collected_r_, 0);
        const auto& p = wrapper.point();
        CHECK(resets_.find(p) != resets_.end());
        auto iter = booster_map_.find(p);
        CHECK(iter == booster_map_.end() || iter->second == Booster::X);
        resets_.insert(p);
        break;
      }
      case Instruction::Type::T: {
        entry.set_action(BacklogEntry::Action::T);
        entry.set_orig_pos(wrapper.point());
        CHECK(resets_.find(inst.arg) != resets_.end());
        wrapper.set_point(inst.arg);
        Fill(wrapper, &entry);
        break;
      }
      case Instruction::Type::C: {
        entry.set_action(BacklogEntry::Action::C);
        CHECK_GT(collected_c_, 0);
        const auto& p = wrapper.point();
        auto iter = booster_map_.find(p);
        CHECK(iter != booster_map_.end() && iter->second == Booster::X);
        wrappers_.emplace_back(p);  // Do not touch wrapper after this.
        break;
      }
    }
  }

  // Consume counter.
  {
    auto& wrapper = wrappers_[index];
    int fast_count = wrapper.fast_count();
    if (fast_count > 0) {
      wrapper.set_fast_count(fast_count - 1);
    }
    int drill_count = wrapper.drill_count();
    if (drill_count > 0) {
      wrapper.set_drill_count(drill_count - 1);
    }
  }

  backlogs_.push_back(std::move(entry));
}

std::string Map::ToString() const {
  std::string result;
  for (int y = static_cast<int>(height_) - 1; y >= 0; --y) {
    for (int x = 0; x < static_cast<int>(width_); ++x) {
      switch ((*this)[Point{x, y}]) {
        case Cell::WALL:
          result.push_back('#');
          break;
        case Cell::EMPTY:
          result.push_back(' ');
          break;
        case Cell::FILLED:
          result.push_back('.');
          break;
      }
    }
    result.push_back('\n');
  }

  // Overwrite booster.
  for (const auto& kv : booster_map_) {
    const auto& p = kv.first;
    int index = (height_ - p.y - 1) * (width_ + 1) + p.x;
    switch (kv.second) {
      case Booster::B:
        result[index] = result[index] == '.' ? 'B' : 'b';
        break;
      case Booster::F:
        result[index] = result[index] == '.' ? 'F' : 'f';
        break;
      case Booster::L:
        result[index] = result[index] == '.' ? 'L' : 'l';
        break;
      case Booster::X:
        result[index] = result[index] == '.' ? 'X' : 'x';
        break;
      case Booster::R:
        result[index] = result[index] == '.' ? 'R' : 'r';
        break;
      case Booster::C:
        result[index] = result[index] == '.' ? 'C' : 'c';
        break;
    }
  }

  // Wrapper.
  for (const auto& wrapper : wrappers_) {
    {
      const auto& p = wrapper.point();
      int index = (height_ - p.y - 1) * (width_ + 1) + p.x;
      result[index] = '%';
    }
    for (const auto& manip : wrapper.manipulators()) {
      const auto& p = wrapper.point() + manip;
      int index = (height_ - p.y - 1) * (width_ + 1) + p.x;
      if (result[index] == ' ' || result[index] == '.') {
        result[index] = '&';
      }
    }
  }

  return result;
}

void Map::Move(Wrapper* wrapper, const Point& direction,
               BacklogEntry* entry,
               BacklogEntry::Action a, BacklogEntry::Action aa) {
  CHECK(MoveInternal(wrapper, direction, entry, true));
  entry->set_action(a);
  if (wrapper->fast_count()) {
    if (MoveInternal(wrapper, direction, entry, false)) {
      entry->set_action(aa);
    }
  }
}

bool Map::MoveInternal(Wrapper* wrapper, const Point& direction,
                       BacklogEntry* entry, bool is_first) {
  auto p = wrapper->point();
  p += direction;
  if (p.x < 0 || static_cast<int>(width_) <= p.x) {
    return false;
  }
  if (p.y < 0 && static_cast<int>(height_) <= p.y) {
    return false;
  }
  if (wrapper->drill_count() == 0) {
    if ((*this)[p] == Cell::WALL) {
      return false;
    }
  }
  wrapper->set_point(p);
  Fill(*wrapper, entry);

  auto iter = booster_map_.find(p);
  if (iter != booster_map_.end()) {
    if (iter->second != Booster::X) {
      booster_map_.erase(p);
      wrapper->set_pending_booster(iter->second);
      if (is_first) {
        entry->set_first_booster(iter->second);
      } else {
        entry->set_second_booster(iter->second);
      }
    }
  }
  return true;
}

void Map::Fill(const Wrapper& wrapper, BacklogEntry* entry) {
  {
    auto& cell = GetCell(wrapper.point());
    if (cell == Cell::EMPTY) {
      --remaining_;
    }
    if (entry)
      entry->add_updated_cell(wrapper.point(), cell);
    cell = Cell::FILLED;
  }
  for (const auto& manip : wrapper.manipulators()) {
    const auto p = wrapper.point() + manip;
    if (!IsVisible(wrapper.point(), p, map_, width_, height_)) {
      continue;
    }
    auto& cell = GetCell(p);
    if (cell != Cell::FILLED) {
      if (entry)
        entry->add_updated_cell(p, cell);
      cell = Cell::FILLED;
      --remaining_;
    }
  }
}

bool Verify(Map* m, const Solution& sol) {
  std::vector<std::size_t> index_list;
  index_list.push_back(0);
  int steps = 0;
  while (true) {
    bool is_ended = true;
    std::size_t size = std::min(index_list.size(), sol.moves.size());
    for (std::size_t i = 0; i < size; ++i) {
      if (index_list[i] < sol.moves[i].size()) {
        is_ended = false;
        break;
      }
    }
    if (is_ended)
      break;

    // Run wrappers in order.
    for (std::size_t i = 0; i < size; ++i) {
      if (index_list[i] < sol.moves[i].size())
        m->Run(i, sol.moves[i][index_list[i]]);
    }

    // Update index list.
    for (std::size_t i = 0; i < size; ++i) {
      if (index_list[i] < sol.moves[i].size()) {
        ++index_list[i];
      }
    }
    while (index_list.size() < m->wrappers().size()) {
      index_list.push_back(0);
    }

    ++steps;
  }

  if (m->remaining() > 0) {
    std::cout << "Failed!: Still remaining "
              << m->remaining() << " at step " << steps;
    return false;
  }

  std::cout << "Success! Passed at step " << steps;
  return true;
}

}  // namepace icfpc2019
