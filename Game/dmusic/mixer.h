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
    void     setVolume(float v);

    void     setMusic(const Music& m);
    int64_t  currentPlayTime() const;

  private:
    struct Active {
      Music::Note* ptr=nullptr;
      int64_t      at=0;

      std::shared_ptr<Music::PatternInternal> pattern; //prevent pattern from deleting
      };

    struct Step final {
      int64_t nextOn =std::numeric_limits<int64_t>::max();
      int64_t nextOff=std::numeric_limits<int64_t>::max();
      int64_t samples=0;
      };

    struct Instr {
      Music::InsInternal* ptr=nullptr;
      float               volLast=1.f;
      std::shared_ptr<Music::PatternInternal> pattern; //prevent pattern from deleting
      };

    Step     stepInc  (Music::PatternInternal &pptn, int64_t b, int64_t e, int64_t samplesRemain);
    void     stepApply(std::shared_ptr<Music::PatternInternal> &pptn, const Step& s, int64_t b);
    void     implMix  (Music::PatternInternal &pptn, float volume, int16_t *out, size_t cnt);

    int64_t  nextNoteOn (Music::PatternInternal &part, int64_t b, int64_t e);
    int64_t  nextNoteOff(int64_t b, int64_t e);

    void     noteOn (std::shared_ptr<Music::PatternInternal> &pattern, Music::Note *r);
    void     noteOn (std::shared_ptr<Music::PatternInternal> &pattern, int64_t time);
    void     noteOff(int64_t time);
    void     setNote(Music::Note& rec, bool on);
    std::shared_ptr<Music::PatternInternal> checkPattern(std::shared_ptr<Music::PatternInternal> p);

    void     nextPattern();
    void     volFromCurve(Music::PatternInternal &part, Instr &ins, std::vector<float> &v);

    std::shared_ptr<Music::Internal> current=nullptr;
    int64_t                          sampleCursor=0;

    std::shared_ptr<Music::PatternInternal> pattern=nullptr;
    int64_t                          patStart=0;
    int64_t                          patEnd  =0;

    std::atomic<float>               volume={1.f};
    std::vector<Active>              active;
    std::vector<Instr>               uniqInstr;
    std::vector<float>               pcm, vol, pcmMix;
  };

}
