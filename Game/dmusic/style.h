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
    std::vector<Band>    band;
    std::vector<Part>    parts;
    std::vector<Pattern> patterns;

    void exec(const Pattern& p) const;
    void mix (const Pattern& p) const;

  private:
    void implRead(Riff &input);
    const Part *findPart(const GUID& guid) const;
    void exec(const Style::Part& part) const;
    void mix(Mixer& m,const Style::Part& part) const;
  };

}
