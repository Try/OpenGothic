#pragma once

#include "info.h"
#include "riff.h"

#include <vector>
#include <cstdint>

#include <Tempest/MemReader>
#include <Tempest/Sound>

namespace Dx8 {

class Wave final {
  public:
    Wave(Riff &input);
    Wave(const char* dbg);
    Wave(const int16_t* pcm,size_t count);

    enum WaveFormatTag : uint16_t {
      UNKNOWN = 0x0000,
      PCM     = 0x0001,
      ADPCM   = 0x0002,
      };

    struct WaveFormat final {
      WaveFormatTag wFormatTag       = WaveFormatTag::UNKNOWN;
      uint16_t      wChannels        = 0;
      uint32_t      dwSamplesPerSec  = 0;
      uint32_t      dwAvgBytesPerSec = 0;
      uint16_t      wBlockAlign      = 0;
      uint16_t      wBitsPerSample   = 0;
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

    struct PoolTable final {
      uint32_t cbSize=0;
      uint32_t cCues =0;
      };

    WaveFormat                  wfmt;
    std::vector<uint8_t>        extra;
    std::vector<uint8_t>        wavedata;
    WaveSample                  waveSample;
    std::vector<WaveSampleLoop> loop;
    Info                        info;

    void toFloatSamples(float* out) const;

    void save(const char* path) const;

  private:
    enum {
      MAX_CACHED_FRAMES=4
      };
    struct AdpcHdrMono final {
      int16_t delta;
      int16_t prevFrames1;
      int16_t prevFrames0;
      };

    struct AdpcHdrStereo final {
      int16_t delta[2];
      int16_t prevFrames1[2];
      int16_t prevFrames0[2];
      };

    struct AdpcChannel final {
      uint16_t predictor;
      int32_t  delta;
      int32_t  prevFrames[2];
      };

    struct AdpcState final {
      int32_t     cachedFrames[MAX_CACHED_FRAMES];  // samples are stored in this cache during decoding.
      uint32_t    cachedFrameCount;
      AdpcChannel channel[2];
      };

    void        implRead(Riff &input);
    void        implParse(Riff &input);

    size_t      decodeAdpcm(Tempest::MemReader& rd, const size_t framesToRead,
                            uint16_t blockAlign, uint16_t channels, int16_t* pBufferOut);
    size_t      decodeAdpcmBlock(Tempest::MemReader& rd, const size_t framesToRead,
                                 uint16_t blockAlign, uint16_t channels, int16_t* pBufferOut);
    int32_t     decodeADPCMFrame(AdpcChannel& msadpcm, int32_t nibble);
  };

}
