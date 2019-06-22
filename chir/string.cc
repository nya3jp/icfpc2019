#include "chir/string.h"

std::vector<std::string> splitAll(std::string s, const std::string&t) {
  std::vector<std::string> v;
  for (size_t p = 0; (p = s.find(t)) != s.npos; ) {
    v.push_back(s.substr(0, p));
    s = s.substr(p + t.size());
  }
  v.push_back(s);
  return v;
}

std::vector<std::string> split(std::string s, const std::string &t) {
  std::vector<std::string> c;
  size_t p = s.find(t);
  if(p != s.npos) {
    c.push_back(s.substr(0,p));
    s = s.substr(p+t.size());
  }
  c.push_back(s);

  return c;
}
