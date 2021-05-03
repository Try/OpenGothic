#pragma once

#include "riff.h"
#include "structs.h"
#include "soundfont.h"
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

    struct WaveSample final {
      uint32_t cbSize      =0;
      uint16_t usUnityNote =0;
      int16_t  sFineTune   =0;
      int32_t  lAttenuation=0;
      uint32_t fulOptions  =0;
      uint32_t cSampleLoops=0;
      };

    struct WaveSampleLoop final {
      uint32_t cbSize      =0;
      uint32_t ulLoopType  =0;
      uint32_t ulLoopStart =0;
      uint32_t ulLoopLength=0;
      };

    class  Region final {
      public:
        Region(Riff& c);

        RegionHeader                head;
        WaveLink                    wlink;
        WaveSample                  waveSample;
        std::vector<WaveSampleLoop> loop;

      private:
        void implRead(Riff &input);
      };

    struct ConnectionBlock final {
      uint16_t  usSource=0;
      uint16_t  usControl=0;
      uint16_t  usDestination=0;
      uint16_t  usTransform=0;
      int32_t   lScale=0;
      };


    class Articulator final {
      public:
        Articulator(Riff& c);
        std::vector<ConnectionBlock> connectionBlocks;
      };

    struct MidiLocale final {
      uint32_t ulBank      =0;
      uint32_t ulInstrument=0;
      };

    struct InstrumentHeader final {
      uint32_t   cRegions=0;
      MidiLocale Locale;
      };

    class  Instrument final {
      public:
        Instrument(Riff& c);

        Info                     info;
        InstrumentHeader         header;
        std::vector<Region>      regions;
        std::vector<Articulator> articulators;

      private:
        void implRead(Riff &input);
      };

    uint64_t                    version=0;
    GUID                        dlid;
    std::vector<Instrument>     instrument;

    void      dbgDump() const;
    SoundFont toSoundfont(uint32_t dwPatch) const;
    void      save(std::ostream& fout) const;

    const Wave* findWave(uint8_t note) const;

  private:
    void implRead(Riff &input);

    std::vector<Wave>                        wave;
    mutable std::shared_ptr<SoundFont::Data> shData; //FIXME: mutable
  };

}
