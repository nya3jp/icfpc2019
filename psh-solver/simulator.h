#ifndef SIMULATOR_H_
#define SIMULATOR_H_

#include <cstdint>
#include <map>
#include <vector>
#include <tuple>

#include "absl/types/optional.h"

namespace icfpc2019 {

struct Point {
  int x;
  int y;

  bool operator<(const Point& other) const {
    return std::tie(x, y) < std::tie(other.x, other.y);
  }

  Point& operator+=(const Point& other) {
    x += other.x;
    y += other.y;
    return *this;
  }

  Point& operator-=(const Point& other) {
    x -= other.x;
    y -= other.y;
    return *this;
  }

  friend Point operator+(const Point& lhs, const Point& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
  }

  friend Point operator-(const Point& lhs, const Point& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y};
  }
};

std::ostream& operator<<(std::ostream& os, const Point& point);

enum class Cell : std::uint8_t {
  EMPTY,
  FILLED,
  WALL,
};

enum class Booster : std::uint8_t {
  B,
  F,
  L,
  X,
  R,
  C,
};

struct Desc {
  std::vector<Point> map_;
  Point point;
  std::vector<std::vector<Point>> obstacles;
  std::vector<std::pair<Point, Booster>> boosters;
};


struct Instruction {
  enum class Type : std::uint8_t {
    W,
    S,
    A,
    D,
    Q,
    E,
    Z,
    B,  // With arg.
    F,
    L,
    R,
    T,  // With arg.
    C,
  };
  Type type;
  Point arg;
};

std::ostream& operator<<(std::ostream& os, const Instruction& inst);
std::ostream& operator<<(std::ostream& os, const Instruction::Type& type);

Desc ParseDesc(const std::string& task);

class Wrapper {
 public:
  explicit Wrapper(const Point& point) : point_(point) {}

  const Point& point() const { return point_; }
  void set_point(const Point& p) { point_ = p; }

  const std::vector<Point>& manipulators() const {
    return manipulators_;
  }

  void RotateClockwise() {
    for (auto& manip : manipulators_) {
      manip = Point{-manip.y, manip.x};
    }
  }

  void RotateCounterClockwise() {
    for (auto& manip : manipulators_) {
      manip = Point{manip.y, -manip.x};
    }
  }

 private:
  Point point_;
  std::vector<Point> manipulators_ = {
    {1, -1},
    {1, 0},
    {1, 1},
  };
};

class Map {
 public:
  explicit Map(const Desc& desc);

  Cell operator[](const Point& p) const {
    return map_[Index(p)];
  }

  absl::optional<Booster> GetBooster(const Point& p) const {
    auto iter = booster_map_.find(p);
    return iter == booster_map_.end() ?
        absl::nullopt : absl::make_optional(iter->second);
  }

  int width() const { return width_; }
  int height() const { return height_; }

  int remaining() const { return remaining_; }

  const std::vector<Wrapper>& wrappers() const { return wrappers_; }

  void Run(int index, const Instruction& inst);

 private:
  inline size_t Index(const Point& p) const {
    return p.y * width_ + p.x;
  }
  Cell& GetCell(const Point& p) {
    return map_[Index(p)];
  }
  void Move(Wrapper* wrapper, const Point& direction);
  void Fill(const Wrapper& wrapper);

  std::size_t width_;
  std::size_t height_;
  std::vector<Cell> map_;
  std::map<Point, Booster> booster_map_;

  int remaining_ = 0;
  std::vector<Wrapper> wrappers_;
};

} // namespace icpfc2019

#endif  // SIMULATOR_H_
