#pragma once

#include "info.h"
#include "riff.h"

#include <vector>
#include <cstdint>

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

    void save(const char* path) const;

  private:
    struct MsState final {
      int32_t step;
      int16_t coef[2];
      };

    void        implRead(Riff &input);
    bool        decodeAdpcm(const uint8_t *adpcm, size_t dataLen, uint16_t samplesPerBlock, uint16_t nCoefs, const int16_t *adpcm_i_coefs);
    uint16_t    adpcmReadBlock(const uint8_t*& adpcm, size_t& dataLen, uint16_t samplesPerBlock, uint16_t nCoefs, const int16_t *adpcm_i_coefs, int16_t *samples);
    const char* adpcmBlockExpand(uint16_t chans, int nCoef, const int16_t* coef, const uint8_t* ibuff, int16_t *obuff, int n);
    int16_t     adpcmDecode(int32_t c, MsState *state, int32_t sample1, int32_t sample2);
  };

}
