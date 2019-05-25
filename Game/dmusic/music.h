#pragma once

#include "segment.h"

namespace Dx8 {

class DirectMusic;

class Music final {
  public:
    Music()=default;
    Music(Music&&)=default;

    Music& operator = (Music&&)=default;

  private:
    Music(const Segment& s,DirectMusic& owner);

  friend class DirectMusic;
  };

}
