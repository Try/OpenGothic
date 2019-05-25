#pragma once

#include <vector>
#include <cstdint>

namespace Dx8 {

class Mixer final {
  public:
    Mixer(size_t sz);

    struct Sample final {
      int16_t l=0,r=0;
      };
    std::vector<Sample> pcm;
  };

}
