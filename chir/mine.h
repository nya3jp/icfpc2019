#ifndef MINE_H_
#define MINE_H_

#include <string>
#include <vector>

#include "chir/string.h"

std::tuple<int, int> parsePoint(const std::string &s) {
  if (s[0] != '(' || s[s.length()-1] != ')'
      || s.find(',') == std::string::npos) {
    std::cerr << "Something wrong!!!!!" << std::endl;
    return std::make_tuple(0, 0);
  }

  std::vector<std::string> nums = split(s.substr(1, s.length() - 2), ",");
  return std::make_tuple(atoi(nums[0].c_str()), atoi(nums[1].c_str()));
}

struct TNode {
  std::vector<std::tuple<int /* depth */, int /* child */>> children;
};

struct Mine {
  void parse(const std::string& input) {
    std::vector<std::string> points = splitAll(input, ")");
    points.pop_back();
    for (size_t i = 1; i < points.size(); ++i) {
      points[i] = points[i].substr(1);
    }

    std::vector<std::tuple<int, int>> ps;
    for (size_t i = 0; i < points.size(); ++i) {
      ps.push_back(parsePoint(points[i] + ")"));
      max_x = std::max(max_x, std::get<0>(ps[i]));
      max_y = std::max(max_y, std::get<1>(ps[i]));
    }

    for (size_t i = 0; i < static_cast<size_t>(max_y); ++i) {
      mp.push_back(std::vector<int>(max_x, 1));
    }

    G bounding;
    for (size_t i = 0; i < ps.size(); ++i) {
      bounding.push_back(P(std::get<0>(ps[i]),
                           std::get<1>(ps[i])));
    }

    for (int i = 0; i < max_y; ++i) {
      for (int j = 0; j < max_x; ++j) {
        if (inside(bounding, P((double)j + 0.5, (double)i + 0.5))) {
          mp[i][j] = 0;
        }
      }
    }
  }

  void fillPolygon(const std::string& input, int color) {
    std::vector<std::string> points = splitAll(input, ")");
    points.pop_back();
    for (size_t i = 1; i < points.size(); ++i) {
      points[i] = points[i].substr(1);
    }

    G poly;
    for (size_t i = 0; i < points.size(); ++i) {
      auto p = parsePoint(points[i]+")");
      poly.push_back(P(std::get<0>(p), std::get<1>(p)));
    }

    for (int i = 0; i < max_y; ++i) {
      for (int j = 0; j < max_x; ++j) {
        if (inside(poly, P((double)j + 0.5, (double)i + 0.5))) {
          mp[i][j] = color;
        }
      }
    }
  }

  void parseObstacles(const std::string& input) {
    std::vector<std::string> obstacle_strs = splitAll(input, ";");
    for (auto s : obstacle_strs)
      fillPolygon(s, 1);
  }

  void setPos(int x, int y) {
    cur_x = x;
    cur_y = y;
    arms = {{0, 0}, {1, -1}, {1, 0}, {1, 1}};
    paint_arms();
  }

  void paint_arms() {
    for (size_t i = 0; i < arms.size(); ++i) {
      int x = cur_x + std::get<0>(arms[i]);
      int y = cur_y + std::get<1>(arms[i]);
      if (x < 0 || x >= max_x) continue;
      if (y < 0 || y >= max_y) continue;
      if (mp[y][x] == 0)
        mp[y][x] = 2;
    }
  }

  void command(const std::string &cmds) {
    for (size_t i = 0; i < cmds.length(); ++i) {
      char cmd = cmds[i];
      switch (cmd) {
        case 'W':
          cur_y++;
          paint_arms();
          break;
        case 'S':
          cur_y--;
          paint_arms();
          break;
        case 'A':
          cur_x--;
          paint_arms();
          break;
        case 'D':
          cur_x++;
          paint_arms();
          break;
        case 'Q':
        case 'E':
          break;
      }
    }
  }

  void dump() {
    std::cout << "pos = (" << cur_x
              << "," << cur_y << ")"
              << std::endl;
    std::cout << "Map:" << std::endl;
    for (size_t i = 0; i < mp.size(); ++i) {
      for (size_t j = 0; j < mp[i].size(); ++j) {
        if (mp[max_y - i - 1][j] == 0) std::cout << '.';
        else if (mp[max_y - i - 1][j] == 1) std::cout << '#';
        else std::cout << 'o';
      }
      std::cout << std::endl;
    }
  }

  // 0 = empty
  // 1 = obstacle
  // 2 = wrapped
  std::vector<std::vector<int>> mp;
  int cur_x = -1;
  int cur_y = -1;
  int max_x = 0;
  int max_y = 0;
  std::vector<std::tuple<int, int>> arms;
};

#endif  // MINE_H_
