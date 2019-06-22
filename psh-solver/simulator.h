#ifndef SIMULATOR_H_
#define SIMULATOR_H_

#include <cstdint>
#include <map>
#include <vector>
#include <set>
#include <tuple>

#include "absl/types/optional.h"

namespace icfpc2019 {

struct Point {
  int x;
  int y;

  friend bool operator==(const Point& lhs, const Point& rhs) {
    return std::tie(lhs.x, lhs.y) == std::tie(rhs.x, rhs.y);
  }
  friend bool operator!=(const Point& lhs, const Point& rhs) {
    return std::tie(lhs.x, lhs.y) != std::tie(rhs.x, rhs.y);
  }
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
std::istream& operator>>(std::istream& is, Point& point);

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

std::istream& operator>>(std::istream& is, Instruction& inst);

Desc ParseDesc(const std::string& task);

class Wrapper {
 public:
  explicit Wrapper(const Point& point) : point_(point) {}

  const Point& point() const { return point_; }
  void set_point(const Point& p) { point_ = p; }

  const std::vector<Point>& manipulators() const {
    return manipulators_;
  }

  void AddManipulator(const Point& p) {
    manipulators_.push_back(p);
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

  int fast_count() const { return fast_count_; }
  void set_fast_count(int count) { fast_count_ = count; }

  int drill_count() const { return drill_count_; }
  void set_drill_count(int count) { drill_count_ = count; }

  Booster pending_booster() const { return pending_booster_; }
  void set_pending_booster(Booster b) { pending_booster_ = b; }

 private:
  Point point_;
  std::vector<Point> manipulators_ = {
    {1, -1},
    {1, 0},
    {1, 1},
  };
  int fast_count_ = 0;
  int drill_count_ = 0;
  Booster pending_booster_ = Booster::X;
};

class BacklogEntry {
 public:
  enum class Action : std::uint8_t {
    W, WW,
    A, AA,
    S, SS,
    D, DD,
    Z,
    Q,
    E,
    B, F, L, R, T, C
  };

  BacklogEntry() = default;

  int wrapper_index() const { return wrapper_index_; }
  void set_wrapper_index(int index) { wrapper_index_ = index; }

  int drill_count() const { return drill_count_; }
  void set_drill_count(int count) { drill_count_ = count; }

  int fast_count() const { return fast_count_; }
  void set_fast_count(int count) { fast_count_ = count; }

  Booster pending_booster() const { return pending_booster_; }
  void set_pending_booster(Booster b) { pending_booster_ = b; }

  Action action() const { return action_; }
  void set_action(Action a) { action_ = a; }

  const std::vector<std::pair<Point, Cell>>& updated_cells() const {
    return updated_cells_;
  }
  void add_updated_cell(const Point& point, const Cell& orig) {
    updated_cells_.emplace_back(point, orig);
  }

  Booster first_booster() const { return first_booster_; }
  void set_first_booster(Booster b) { first_booster_ = b; }

  Booster second_booster() const { return second_booster_; }
  void set_second_booster(Booster b) { second_booster_ = b; }

 private:
  // TODO using shorter bitwidth ints.
  int wrapper_index_;
  int drill_count_;
  int fast_count_;
  Booster pending_booster_ = Booster::X;
  Action action_;
  Booster first_booster_ = Booster::X;
  Booster second_booster_ = Booster::X;

  std::vector<std::pair<Point, Cell>> updated_cells_;
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
  int num_steps() const { return num_steps_; }

  const std::vector<Wrapper>& wrappers() const { return wrappers_; }

  void Run(int index, const Instruction& inst);
  void Undo();

  std::string ToString() const;

 private:
  inline size_t Index(const Point& p) const {
    return p.y * width_ + p.x;
  }
  Cell& GetCell(const Point& p) {
    return map_[Index(p)];
  }
  void Move(Wrapper* wrapper, const Point& direction,
            BacklogEntry* log_entry,
            BacklogEntry::Action a, BacklogEntry::Action aa);
  bool MoveInternal(Wrapper* wrapper, const Point& direction,
                    BacklogEntry* log_entry, bool is_first);
  void Fill(const Wrapper& wrapper, BacklogEntry* entry);

  std::size_t width_;
  std::size_t height_;
  std::vector<Cell> map_;
  std::map<Point, Booster> booster_map_;
  std::set<Point> resets_;

  int num_steps_ = 0;
  int remaining_ = 0;
  std::vector<Wrapper> wrappers_;

  int collected_b_ = 0;
  int collected_f_ = 0;
  int collected_l_ = 0;
  int collected_r_ = 0;
  int collected_c_ = 0;

  // Maybe deque to limit num backtrackable for performance?
  std::vector<BacklogEntry> backlogs_;
};

} // namespace icpfc2019

#endif  // SIMULATOR_H_
