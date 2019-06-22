#include "simulator.h"

#include <algorithm>

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
  for (auto token : absl::StrSplit(s, ')')) {
    int start = token[0] == ',' ? 2 : 1;
    token = token.substr(start, token.length() - start);
    result.push_back(ParsePoint(token));
  }
  return result;
}

std::vector<std::vector<Point>> ParseObstacles(absl::string_view s) {
  std::vector<std::vector<Point>> result;
  for (auto token : absl::StrSplit(s, ';')) {
    result.push_back(ParseMap(token));
  }
  return result;
}

std::vector<std::pair<Point, Booster>> ParseBoosters(absl::string_view s) {
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
    if (s.x < g.x) {
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
    if (m[p.y * width + p.x] == Cell::WALL)
      return false;
  }
  return true;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const Point& point) {
  return os << "(" << point.x << "," << point.y << ")";
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
  Fill(wrappers_[0]);
  remaining_ = std::count(map_.begin(), map_.end(), Cell::EMPTY);
}

void Map::Run(int index, const Instruction& inst) {
  // TODO record back log.
  CHECK_LT(index, static_cast<int>(wrappers_.size()));
  auto& wrapper = wrappers_[index];
  switch (inst.type) {
    case Instruction::Type::W:
      Move(&wrapper, Point{0, 1});
      break;
    case Instruction::Type::S:
      Move(&wrapper, Point{0, -1});
      break;
    case Instruction::Type::A:
      Move(&wrapper, Point{-1, 0});
      break;
    case Instruction::Type::D:
      Move(&wrapper, Point{1, 0});
      break;
    case Instruction::Type::Q:
      wrapper.RotateCounterClockwise();
      Fill(wrapper);
      break;
    case Instruction::Type::E:
      wrapper.RotateClockwise();
      Fill(wrapper);
      break;
    case Instruction::Type::Z:
      break;
    case Instruction::Type::B:  // With arg.
    case Instruction::Type::F:
    case Instruction::Type::L:
    case Instruction::Type::R:
    case Instruction::Type::T:  // With arg.
    case Instruction::Type::C:
      LOG(FATAL) << "Unknown/Unsupported Instruction Type: " << inst;
  }
}

void Map::Move(Wrapper* wrapper, const Point& direction) {
  auto p = wrapper->point();
  p += direction;
  CHECK(0 <= p.x && p.x < static_cast<int>(width_)) << p.x;
  CHECK(0 <= p.y && p.y < static_cast<int>(height_)) << p.y;
  CHECK((*this)[p] != Cell::WALL);  // TODO drill.
  wrapper->set_point(p);
  Fill(*wrapper);
  // TODO booster collect.
}

void Map::Fill(const Wrapper& wrapper) {
  {
    // TODO drill.
    auto& cell = GetCell(wrapper.point());
    if (cell != Cell::FILLED) {
      cell = Cell::FILLED;
      --remaining_;
    }
  }
  for (const auto& manip : wrapper.manipulators()) {
    const auto p = wrapper.point() + manip;
    if (!IsVisible(wrapper.point(), p, map_, width_, height_)) {
      continue;
    }
    auto& cell = GetCell(p);
    if (cell != Cell::FILLED) {
      cell = Cell::FILLED;
      --remaining_;
    }
  }
}

}  // namepace icfpc2019
