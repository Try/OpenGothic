#pragma once

#include "reference.h"
#include "riff.h"
#include "structs.h"

#include <vector>

namespace Dx8 {

class Band final {
  public:
    Band(Riff &input);

    struct Instrument final {
      Instrument(Riff &input);

      DMUS_IO_INSTRUMENT header;
      Reference          reference;
      };

    GUID                    guid;
    DMUS_IO_VERSION         vers;
    std::vector<Instrument> intrument;

  private:
    void implRead(Riff &input);
  };

}
