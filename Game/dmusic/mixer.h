#pragma once

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>

#include <vector>
#include <cstdint>
#include <thread>
#include <atomic>

#include "music.h"

namespace Dx8 {

class Mixer final {
  public:
    Mixer();
    ~Mixer();

    void     mix(int16_t *out, size_t samples);

    void     setMusic(const Music& m);
    int64_t  currentPlayTime() const;

  private:
    struct Active {
      Music::Note* ptr=nullptr;
      int64_t      at=0;
      };

    struct Step final {
      int64_t nextOn =std::numeric_limits<int64_t>::max();
      int64_t nextOff=std::numeric_limits<int64_t>::max();
      int64_t samples=0;
      };

    void     thFunc();
    Step     stepInc  (Music::PatternInternal &part, int64_t b, int64_t e, int64_t samplesRemain);
    void     stepApply(Music::PatternInternal &part, const Step& s, int64_t b);
    void     implMix  (Music::PatternInternal &part, int16_t *out, size_t cnt);

    int64_t  nextNoteOn (Music::PatternInternal &part, int64_t b, int64_t e);
    int64_t  nextNoteOff(int64_t b, int64_t e);

    void     noteOn (Music::Note *r);
    void     noteOn (Music::PatternInternal &part, int64_t time);
    void     noteOff(int64_t time);
    void     setNote(Music::Note& rec, bool on);

    void     nextPattern();
    void     volFromCurve(Music::PatternInternal &part, size_t instId, const Music::InsInternal *ins, std::vector<float> &v);

    std::atomic<bool>  exitFlg;
    std::thread        mixTh;

    std::shared_ptr<Music::Internal> current=nullptr;
    int64_t                          sampleCursor=0;

    std::shared_ptr<Music::PatternInternal> pattern=nullptr;
    int64_t                          patStart=0;
    int64_t                          patEnd  =0;

    float                            volume=1.f;
    std::vector<Active>              active;
    std::vector<float>               pcm, vol, pcmMix;
    std::vector<float>               volPerInst;
  };

}
