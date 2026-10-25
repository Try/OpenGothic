#pragma once

#include <algorithm>

namespace SafeArea {
inline int position(int position, int size, int first, int end) {
  const int last = end-size;
  // Move only when clipped. Oversized content covers the safe interval.
  return std::clamp(position, std::min(first,last), std::max(first,last));
  }

inline int center(int extent, int size, int leading, int trailing) {
  return position((extent-size)/2,size,leading,extent-trailing);
  }
}
