
#pragma once

class Math
{
public:
  template<typename T> static const T& max(const T& a, const T& b) {return a > b ? a : b;}
  template<typename T> static const T& min(const T& a, const T& b) {return a < b ? a : b;}
};
