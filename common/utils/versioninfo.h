#pragma once

#include <cstdint>

class VersionInfo final {
  public:
    uint8_t game =2;
    int32_t patch=0;

    bool     hasZSStateLoop()     const { return game==2 && patch>=5; }
    uint16_t dialogGestureCount() const { return game==2 ? 11 : 21;   }
  };

