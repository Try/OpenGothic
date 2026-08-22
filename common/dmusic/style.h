#pragma once

#include "band.h"
#include "info.h"
#include "pattern.h"
#include "riff.h"
#include "structs.h"

#include <vector>

namespace Dx8 {

class Mixer;

class Style final {
  public:
    Style(Riff& riff);

    struct Part final {
      Part(Riff& riff);

      DMUS_IO_STYLEPART               header;
      std::vector<DMUS_IO_STYLENOTE>  notes;
      std::vector<DMUS_IO_STYLECURVE> curves;
      };

    GUID                 guid;
    DMUS_IO_STYLE        styh;
    std::vector<Band>    band;
    std::vector<Part>    parts;
    std::vector<Pattern> patterns;

    const Part *findPart(const GUID& guid) const;

  private:
    void implRead(Riff &input);
  };

}
