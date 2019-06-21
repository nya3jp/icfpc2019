#ifndef INTERSECTION_H_
#define INTERSECTION_H_

#include "geometry.h"
using namespace std;

template <typename T> inline constexpr
int signum(T x, std::false_type is_signed) {
  return T(0) < x;
}

template <typename T> inline constexpr
int signum(T x, std::true_type is_signed) {
  return (T(0) < x) - (x < T(0));
}

template <typename T> inline constexpr
int signum(T x) {
  return signum(x, std::is_signed<T>());
}

bool intersect_ll(const L &l, const L &m) {
    return (abs(outerProduct(l[1], m[1])) > eps || abs(outerProduct(l[1], m[0]-l[0])) < eps);
}

bool intersect_ls(const L &l, const L &s) {
    return (signum(outerProduct(l[1], s[0]-l[0])) *
            signum(outerProduct(l[1], s[0]+s[1]-l[0])) <= 0);
}

bool intersect_ss(const L &s,const L &t) {
    return (ccw(s[0], s[0]+s[1], t[0]) *
            ccw(s[0], s[0]+s[1], t[0]+t[1]) <= 0 &&
            ccw(t[0], t[0]+t[1], s[0]) *
            ccw(t[0], t[0]+t[1], s[0]+s[1]) <= 0);
}

bool intersect_sp(const L &s,const P &p) {
    return (abs(s[0]-p) + abs(s[0]+s[1]-p) - abs(s[1]) < eps);
}

bool inside(const G &g,const P &p) {
    double sum = 0.0;
    int n = g.size();
    for(int i=0; i<n; ++i) {
        int j = (i+1)%n;
        if(intersect_sp(L(g[i], g[j]-g[i]), p))
            return true;
        sum += arg((g[j]-p)/(g[i]-p));
    }
    return (abs(sum) > 1);
}

// circle と line の交点
vector<P> cross_cl(const C& c, const L& l) {
  P h = perf(l, c.p);
  double d = abs(h - c.p);
  vector<P> res;
  if(d < c.r - eps){
    P x = l[1] / abs(l[1]) * sqrt(c.r*c.r - d*d);
    res.push_back(h + x);
    res.push_back(h - x);
  }else if(d < c.r + eps){
    res.push_back(h);
  }
  return res;
}

P cross_ll(const L &l,const L &m) {
    double num = outerProduct(m[1], m[0]-l[0]);
    double denom = outerProduct(m[1], l[1]);
    return P(l[0] + l[1]*num/denom);
}

#endif  // INTERSERCTION_H_
