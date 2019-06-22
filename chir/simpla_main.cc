#include <algorithm>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

#include "gflags/gflags.h"

#include "chir/geometry.h"
#include "chir/intersection.h"
#include "chir/mine.h"
#include "chir/string.h"

DEFINE_string(input, "", "input file path");

void dfs_rec(Mine &mine, std::vector<std::vector<int>> &np, std::vector<TNode> &tree, int x, int y) {
  int idx = y * mine.max_x + x;
  np[y][x] = 10;
  int dx[4] = {0, 0, 1, -1};
  int dy[4] = {1, -1, 0, 0};
  for (int i = 0; i < 4; ++i) {
    int nx = x + dx[i], ny = y + dy[i];
    if (nx < 0 || nx >= mine.max_x || ny < 0 || ny >= mine.max_y
        || np[ny][nx] != 0) continue;
    tree[idx].children.push_back(std::make_tuple(0, ny * mine.max_x + nx));
    dfs_rec(mine, np, tree, nx, ny);
  }
}

int calculate_depth(Mine &mine,
                    std::vector<TNode> &tree,
                    int x, int y) {
  int idx = y * mine.max_x + x;
  if (tree[idx].children.empty()) return 1;
  int max_d = 0;
  for (auto &child : tree[idx].children) {
    int c = std::get<1>(child);
    int nx = c % mine.max_x, ny = c / mine.max_x;
    int d = calculate_depth(mine, tree, nx, ny);
    std::get<0>(child) = d;
    max_d = std::max(max_d, d);
  }
  return max_d + 1;
}

void construct_path(Mine &mine,
                    std::vector<TNode> &tree,
                    int x, int y, int &remaining) {
  int idx = y * mine.max_x + x;
  if (tree[idx].children.empty()) return;
  int dx[4] = {0, 0, 1, -1};
  int dy[4] = {1, -1, 0, 0};
  char dir[4] = {'W', 'S', 'D', 'A'};
  char rev[4] = {'S', 'W', 'A', 'D'};
  std::sort(tree[idx].children.begin(),
            tree[idx].children.end());

  for (auto &child : tree[idx].children) {
    int c = std::get<1>(child);
    int nx = c % mine.max_x, ny = c / mine.max_x;
    for (int i = 0; i < 4; ++i) {
      if (x + dx[i] == nx && y + dy[i] == ny) {
        std::cout << dir[i];
        remaining--;
        construct_path(mine, tree, nx, ny, remaining);
        if (remaining == 1) return;
        std::cout << rev[i];
      }
    }
  }
}

void dfs(Mine &mine) {
  std::vector<std::vector<int>> np = mine.mp;
  std::vector<TNode> tree(mine.max_x * mine.max_y, TNode{});
  dfs_rec(mine, np, tree, mine.cur_x, mine.cur_y);

  calculate_depth(mine, tree, mine.cur_x, mine.cur_y);
  int remaining = 0;
  for (int i = 0; i < mine.max_y; ++i)
    for (int j = 0; j < mine.max_x; ++j)
      if (mine.mp[i][j] == 0) remaining++;
  construct_path(mine, tree, mine.cur_x, mine.cur_y, remaining);
}

std::tuple<P, P> sweep_to_right(Mine &mine, int sx, int sy) {
  P start(sx, sy);
  int gx = sx;
  int army[3] = {-1, 0, 1};
  for (int x = sx; x < mine.max_x; ++x) {
    if (x >= mine.max_x || mine.mp[sy][x] != 0) {
      gx = x - 1;
      break;
    }
    gx = x;
    for (int i = 0; i < 3; ++i) {
      int y = sy + army[i];
      if (y >= 0 && y < mine.max_y && mine.mp[y][x] == 0) {
        mine.mp[y][x] = 2;
      }
    }
  }
  return std::make_tuple(start, P(gx, sy));
}

std::vector<std::tuple<P, P>> find_segments(Mine &mine) {
  // Find 3x horizontal bands
  std::vector<std::tuple<P, P>> segments;
  for (int i = 0; i < mine.max_y; ++i) {
    for (int j = 0; j < mine.max_x; ++j) {
      if (mine.mp[i][j] == 0 && i+1 < mine.max_y &&
          mine.mp[i+1][j] == 0) {
        // Start from (j, i+1)
        segments.push_back(sweep_to_right(mine, j, i+1));
      } else if (mine.mp[i][j] == 0) {
        segments.push_back(sweep_to_right(mine, j, i));
      }
    }
  }

  for (int i = 0; i < mine.max_y; ++i)
    for (int j = 0; j < mine.max_x; ++j)
      if (mine.mp[i][j] == 2)
        mine.mp[i][j] = 0;
  return segments;
}

std::tuple<P, P, P, std::string> find_nearest_segments(
    Mine& mine, std::vector<std::tuple<P, P>> &segments) {
  int dx[4] = {0, 0, 1, -1};
  int dy[4] = {1, -1, 0, 0};
  char dir[4] = {'W', 'S', 'D', 'A'};

  std::vector<std::vector<int>> mp(mine.max_y, std::vector<int>(mine.max_x, -1));
  for (int i = 0; i < mine.max_y; ++i) {
    for (int j = 0; j < mine.max_x; ++j) {
      if (mine.mp[i][j] == 0 || mine.mp[i][j] == 2)
        mp[i][j] = 0;
    }
  }

  mp[mine.cur_y][mine.cur_x] = 2;
  std::queue<std::tuple<int, int, std::string>> q;
  q.push(std::make_tuple(mine.cur_x, mine.cur_y, ""));
  while (!q.empty()) {
    auto t = q.front();
    q.pop();

    for (int i = 0; i < 4; ++i) {
      int x = std::get<0>(t) + dx[i];
      int y = std::get<1>(t) + dy[i];
      if (x < 0 || x >= mine.max_x || y < 0 || y >= mine.max_y || mp[y][x] != 0)
        continue;

      for (size_t j = 0; j < segments.size(); ++j) {
        auto start = std::get<0>(segments[j]);
        auto end = std::get<1>(segments[j]);
        if (start == P(x, y)) {
          return std::make_tuple(start, start, end, std::get<2>(t) + dir[i]);
        }
        if (end == P(x, y)) {
          return std::make_tuple(end, start, end, std::get<2>(t) + dir[i]);
        }
      }
      mp[y][x] = 2;
      q.push(std::make_tuple(x, y, std::get<2>(t) + dir[i]));
    }
  }
  std::cout << "Couldn't find the segments" << std::endl;
  return std::make_tuple(P(0,0), P(0,0), P(0,0), "");
}

std::string find_path(Mine& mine, std::vector<std::tuple<P, P>> &segs) {
  std::string cmds = "";
  while (!segs.empty()) {
    auto t = find_nearest_segments(mine, segs);
    auto now = std::get<0>(t);
    auto then = (now == std::get<1>(t)) ? std::get<2>(t) : std::get<1>(t);

    int now_x = real(now), now_y = imag(now);
    if (now_y - 1 >= 0 && mine.mp[now_y - 1][now_x] == 0) {
      cmds += "QE";
      mine.mp[now_y - 1][now_x] = 2;
    }
    if (now_y + 1 < mine.max_y && mine.mp[now_y + 1][now_x] == 0) {
      cmds += "EQ";
      mine.mp[now_y + 1][now_x] = 2;
    }
    cmds += std::get<3>(t);
    mine.command(std::get<3>(t));
    int then_x = real(then), then_y = imag(then);
    if (then_y - 1 >= 0 && mine.mp[then_y - 1][then_x] == 0) {
      cmds += "QE";
      mine.mp[then_y - 1][then_x] = 2;
    }
    if (then_y + 1 < mine.max_y && mine.mp[then_y + 1][then_x] == 0) {
      cmds += "EQ";
      mine.mp[then_y + 1][then_x] = 2;
    }
    for (size_t i = 0; i < segs.size(); ++i) {
      if (std::get<0>(segs[i]) == std::get<1>(t) &&
          std::get<1>(segs[i]) == std::get<2>(t)) {
        segs.erase(segs.begin() + i);
        break;
      }
    }

    if (real(now) <= real(then)) {
      auto s = std::string(real(then) - real(now), 'D');
      mine.command(s);
      cmds += s;
    } else {
      auto s = std::string(real(now) - real(then), 'A');
      mine.command(s);
      cmds += s;
    }
    mine.cur_x = real(then);
    mine.cur_y = imag(then);
  }
  return cmds;
}

int main(int argc, char** argv) {
  gflags::SetUsageMessage("solver");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_input.empty()) {
    std::cerr << "No input file is specified" << std::endl;
    return 1;
  }

  std::ifstream ifs(FLAGS_input.c_str());
  std::string input;
  std::getline(ifs, input);
  std::vector<std::string> parts = splitAll(input, "#");

  Mine mine;
  mine.parse(parts[0]);
  mine.parseObstacles(parts[2]);
  std::tuple<int, int> initial_pos = parsePoint(parts[1]);
  mine.setPos(std::get<0>(initial_pos), std::get<1>(initial_pos));
  // dfs(mine);
  auto segments = find_segments(mine);
  std::cout << find_path(mine, segments) << std::endl;

  // mine.dump();

  return 0;
}
