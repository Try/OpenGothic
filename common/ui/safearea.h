#pragma once

#include <algorithm>
#include <Tempest/Widget>

namespace SafeArea {
inline Tempest::Rect visibleRect(const Tempest::Widget& widget) {
  const auto* parent = widget.owner();
  const int width = std::max(1,std::min(widget.w(),parent ? parent->w()-widget.x() : widget.w()));
  const int height = std::max(1,std::min(widget.h(),parent ? parent->h()-widget.y() : widget.h()));
  const auto& margin = widget.margins();
  const int left = std::clamp(margin.left,0,width-1);
  const int top = std::clamp(margin.top,0,height-1);
  // Apply insets to the visible viewport when an opening-size modal outlives a resize.
  Tempest::Rect area(left,top,std::max(1,width-left-margin.right),std::max(1,height-top-margin.bottom));
  if(parent) {
    auto bounds = parent->clientRect();
    bounds.x -= widget.x();
    bounds.y -= widget.y();
    area = area.intersected(bounds);
    }
  const int x = std::clamp(area.x,0,width-1), y = std::clamp(area.y,0,height-1);
  return {x,y,std::clamp(area.w,1,width-x),std::clamp(area.h,1,height-y)};
  }

inline int position(int position, int size, int first, int end) {
  const int last = end-size;
  // Move only when clipped. Oversized content covers the safe interval.
  return std::clamp(position, std::min(first,last), std::max(first,last));
  }

inline int center(int extent, int size, int leading, int trailing) {
  return position((extent-size)/2,size,leading,extent-trailing);
  }
}
