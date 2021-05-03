#pragma once

#include <vector>
#include "riff.h"
#include "track.h"

namespace Dx8 {

class Segment final {
  public:
    Segment()=default;
    Segment(Riff& input);

    std::vector<Track> track;

  private:
    void implReadList(Riff &ch);
  };

}
