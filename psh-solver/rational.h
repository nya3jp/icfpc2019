#ifndef RATIONAL_H_
#define RATIONAL_H_

#include <tuple>

namespace icfpc2019 {

class Rational {
 public:
  explicit Rational(int num);
  Rational(int num, int den);

  int num;
  int den;

  Rational& operator+=(const Rational& other);
  Rational& operator-=(const Rational& other);
  Rational& operator*=(const Rational& other);
  Rational& operator/=(const Rational& other);
};

Rational operator+(const Rational& lhs, const Rational& rhs);
Rational operator-(const Rational& lhs, const Rational& rhs);
Rational operator*(const Rational& lhs, const Rational& rhs);
Rational operator/(const Rational& lhs, const Rational& rhs);

inline bool operator==(const Rational& lhs, const Rational& rhs) {
  return std::tie(lhs.num, lhs.den) == std::tie(rhs.num, rhs.den);
}
inline bool operator!=(const Rational& lhs, const Rational& rhs) {
  return !(lhs == rhs);
}
#define OP_DEF(op) \
inline bool operator op (const Rational& lhs, const Rational& rhs) { \
  return lhs.num * rhs.den op rhs.num * lhs.den; \
}
OP_DEF(<)
OP_DEF(<=)
OP_DEF(>)
OP_DEF(>=)
#undef OP_DEF

int Floor(const Rational& r);
int Ceil(const Rational& r);

}  // namespace icfpc2019

#endif  // RATIONAL_H_
