#pragma once

#include "riff.h"
#include "structs.h"
#include "wave.h"

#include <vector>

namespace Dx8 {

class DlsCollection final {
  public:
    DlsCollection(Riff &input);

    struct RgnRange final {
      uint16_t usLow =0;
      uint16_t usHigh=0;
      };

    struct RegionHeader final {
      RgnRange RangeKey;
      RgnRange RangeVelocity;
      uint16_t fusOptions=0;
      uint16_t usKeyGroup=0;
      };

    struct WaveLink final {
      uint16_t fusOptions  =0;
      uint16_t usPhaseGroup=0;
      uint32_t ulChannel   =0;
      uint32_t ulTableIndex=0;
      };

    class Region final {
      public:
        Region(Riff& c);

        RegionHeader head;
        WaveLink     wlink;

      private:
        void implRead(Riff &input);
      };

    struct MidiLocale final {
      uint32_t ulBank      =0;
      uint32_t ulInstrument=0;
      };

    struct InstrumentHeader final {
      uint32_t   cRegions=0;
      MidiLocale Locale;
      };

    class Instrument final {
      public:
        Instrument(Riff& c);

        Info                info;
        InstrumentHeader    header;
        std::vector<Region> regions;

      private:
        void implRead(Riff &input);
      };

    uint64_t                    version=0;
    GUID                        dlid;
    std::vector<Tempest::Sound> wave;
    std::vector<Instrument>     instrument;

    void exec() const;

    const Tempest::Sound *findWave(uint8_t note) const;

  private:
    void implRead(Riff &input);
  };

}
