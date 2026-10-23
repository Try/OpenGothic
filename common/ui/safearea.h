#pragma once

#include <algorithm>

inline int safeAreaPosition(int position, int size, int first, int end) {
  const int last = end-size;
  // Move only when clipped. Oversized content covers the safe interval.
  return std::clamp(position, std::min(first,last), std::max(first,last));
  }

inline int safeAreaCenter(int extent, int size, int leading, int trailing) {
  return safeAreaPosition((extent-size)/2,size,leading,extent-trailing);
  }
