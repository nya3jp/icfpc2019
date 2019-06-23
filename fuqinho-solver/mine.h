#ifndef MINE_H_
#define MINE_H_

#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <algorithm>
#include <queue>
#include <iostream>
#include <sstream>
#include <vector>
#include <queue>
#include <stack>
#include <set>
#include <map>
#include <bitset>
#include <algorithm>
#include <numeric>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cassert>
#include <random>
#include "fuqinho-solver/geometry.h"
#include "fuqinho-solver/intersection.h"

std::vector<std::string> splitAll(std::string s, std::string t){
  std::vector<std::string> v;
  for(size_t p=0; (p = s.find(t)) != s.npos; ){
    v.push_back(s.substr(0,p));
    s=s.substr(p+t.size());
  }
  v.push_back(s);
  return v;
}

std::vector<std::string> split(std::string s, std::string t) {
  std::vector<std::string> c;
  size_t p = s.find(t);
  if(p != s.npos) {
    c.push_back(s.substr(0,p));
    s = s.substr(p+t.size());
  }
  c.push_back(s);

  return c;
}

std::tuple<int, int> parsePoint(const std::string &s) {
  if (s[0] != '(' || s[s.length()-1] != ')'
      || s.find(',') == std::string::npos) {
    std::cerr << "Something wrong!!!!!" << std::endl;
    return std::make_tuple(0, 0);
  }

  std::vector<std::string> nums = split(s.substr(1, s.length() - 2), ",");
  return std::make_tuple(atoi(nums[0].c_str()), atoi(nums[1].c_str()));
}

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
    // arms = {{0, 0}, {1, -1}, {1, 0}, {1, 1}};
    // for (size_t i = 0; i < arms.size(); ++i) {
    //   int x = cur_x + std::get<0>(arms[i]);
    //   int y = cur_y + std::get<1>(arms[i]);
    //   if (x < 0 || x >= max_x) continue;
    //   if (y < 0 || y >= max_y) continue;
    //   mp[y][x] = 2;
    // }
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
  std::string boosters;
};

#endif  // MINE_H_
