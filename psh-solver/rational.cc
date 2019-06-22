#include "rational.h"

#include "glog/logging.h"

namespace icfpc2019 {
namespace {
int Gcd(int a, int b) {
  if (a < b) {
    std::swap(a, b);
  }

  while (true) {
    int r = a % b;
    if (r == 0) return b;
    a = b;
    b = r;
  }
}
}  // namespace

Rational::Rational(int num) : num(num), den(1) {}

Rational::Rational(int num, int den) {
  CHECK_GT(den, 0);
  if (num == 0) {
    den = 1;
  } else {
    int gcd = Gcd(std::abs(num), den);
    den /= gcd;
    num /= gcd;
  }
  this->den = den;
  this->num = num;
}

Rational operator+(const Rational& lhs, const Rational& rhs) {
  return Rational(lhs.num * rhs.den + rhs.num * lhs.den,
                  lhs.den * rhs.den);
}

Rational operator-(const Rational& lhs, const Rational& rhs) {
  return Rational(lhs.num * rhs.den - rhs.num * lhs.den,
                  lhs.den * rhs.den);
}

Rational operator*(const Rational& lhs, const Rational& rhs) {
  return Rational(lhs.num * rhs.num,
                  lhs.den * rhs.den);
}

Rational operator/(const Rational& lhs, const Rational& rhs) {
  CHECK(rhs.num != 0);
  int num = lhs.num * rhs.den;
  int den = lhs.den * rhs.num;
  if (den < 0) {
    num = -num;
    den = -den;
  }
  return Rational(num, den);
}

Rational& Rational::operator+=(const Rational& other) {
  return *this = *this + other;
}

Rational& Rational::operator-=(const Rational& other) {
  return *this = *this - other;
}

Rational& Rational::operator*=(const Rational& other) {
  return *this = *this * other;
}

Rational& Rational::operator/=(const Rational& other) {
  return *this = *this / other;
}

int Floor(const Rational& r) {
  int result = r.num / r.den;
  if (r.num % r.den < 0) {
    --result;
  }
  return result;
}

int Ceil(const Rational& r) {
  int result = r.num / r.den;
  if (r.num % r.den > 0) {
    ++result;
  }
  return result;
}

}  // namespace icfpc2019
