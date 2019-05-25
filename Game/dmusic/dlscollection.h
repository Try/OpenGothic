#pragma once

#include "riff.h"
#include "structs.h"
#include "wave.h"

#include <vector>

namespace Dx8 {

class DlsCollection final {
  public:
    DlsCollection(Riff &input);

    class Region final {
      public:
        Region(Riff& c);
      };

    class Instrument final {
      public:
        Instrument(Riff& c);

        GUID                dlsid;
        uint32_t            midiBank=0;
        uint32_t            midiProgram=0;
        std::vector<Region> regions;

      private:
        void implRead(Riff &input);
      };

    std::uint64_t           version=0;
    GUID                    dlid;
    std::vector<Wave>       wave;
    std::vector<Instrument> instrument;

  private:
    void implRead(Riff &input);
  };

}
