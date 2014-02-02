
#pragma once

class Math
{
public:
  template<typename T> static const T& max(const T& a, const T& b) {return a > b ? a : b;}
  template<typename T> static const T& min(const T& a, const T& b) {return a < b ? a : b;}
  template<typename T> static const T abs(const T& v) {return v < 0 ? -v : v;} // TODO: fast abs for double/float
};
