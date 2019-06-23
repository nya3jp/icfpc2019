#ifndef GEOMETRY_H_
#define GEOMETRY_H_

#include<iostream>
#include<complex>
#include<vector>
using namespace std;

const double eps = 1e-8;
const double inf = 1e12;

typedef complex<double> P; // point

namespace std {
bool operator<(const complex<double>& p, const complex<double>&q) {
  return real(p) != real(q) ? real(p) < real(q) : imag(p) < imag(q);
}
}  // namespace std

struct L : public vector<P> { // line
    L (P a, P b) {
        push_back(a);
        push_back(b);
    }
};

struct C { // circle
    P p;
    double r;
    C(P c, double r) : p(c), r(r) {;}
};

// return a*b
inline double innerProduct(const P &a,const P &b) {
    return (conj(a)*b).real();
}


// return |a×b|
inline double outerProduct(const P &a,const P &b) {
    return (conj(a)*b).imag();
}

// ベクトルpをベクトルbに射影したベクトル
inline P proj(const P &p,const P &b) {
    return b*innerProduct(p,b)/norm(b);
}

// 点pから直線lに引いた垂線の足
inline P perf(const L &l,const P &p) {
    L m = L(l[0]-p,l[1]);
    return (p + (m[0] -  proj(m[0], m[1])));
}

// 線分sを直線bに射影した線分
inline L proj(const L &s,const L &b) {
    return L(perf(b,s[0]), proj(s[1], b[1]));
}

int ccw(P a,P b,P c) {
    b -= a; c -= a;
    if(outerProduct(b,c) > 0) return 1; //ccw
    if(outerProduct(b,c) < 0) return -1; //cw
    if(innerProduct(b,c) < 0) return 2; // c--a--b
    if(abs(b) < abs(c)) return -2; // a--b--c
    return 0;
}

typedef vector<P> G; //geometry

#endif // GEOMETRY_H_
