#pragma once

#include "info.h"
#include "riff.h"
#include "structs.h"

namespace Dx8 {

class Pattern final {
  public:
    Pattern(Riff &riff);

    struct PartRef final {
      PartRef(Riff &input);
      DMUS_IO_PARTREF io;
      Unfo            unfo;
      };

    DMUS_IO_PATTERN      header;
    Unfo                 info;
    std::vector<PartRef> partref;

    uint32_t             timeLength(double tempo) const;

  private:
    void                 implRead(Riff &riff);
  };

}
