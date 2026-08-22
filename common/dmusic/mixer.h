#pragma once

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>

#include <vector>
#include <cstdint>
#include <thread>
#include <atomic>
#include <list>

#include "patternlist.h"
#include "music.h"

namespace Dx8 {

class Mixer final {
  public:
    Mixer();
    ~Mixer();

    void     mix(int16_t *out, size_t samples);
    void     setVolume(float v);

    void     setMusic(const Music& m,DMUS_EMBELLISHT_TYPES embellishment=DMUS_EMBELLISHT_NORMAL);
    void     setMusicVolume(float v);
    int64_t  currentPlayTime() const;

  private:
    struct Instr;

    struct Active {
      int64_t           at=0;
      SoundFont::Ticket ticket;
      Instr*            parent=nullptr;
      };

    struct Step final {
      int64_t nextOn =std::numeric_limits<int64_t>::max();
      int64_t nextOff=std::numeric_limits<int64_t>::max();
      int64_t samples=0;
      bool    isValid() const { return !(nextOn==std::numeric_limits<int64_t>::max() && nextOn==nextOff && samples==0); }
      };

    struct Instr {
      PatternList::InsInternal* ptr=nullptr;
      float                     volLast=1.f;
      size_t                    counter=0;
      std::shared_ptr<PatternList::PatternInternal> pattern; //prevent pattern from deleting
      };

    using PatternInternal = PatternList::PatternInternal;

    Step     stepInc  (PatternInternal &pptn, int64_t b, int64_t e, int64_t samplesRemain);
    void     stepApply(std::shared_ptr<PatternList::PatternInternal> &pptn, const Step& s, int64_t b);
    void     implMix  (PatternList::PatternInternal &pptn, float volume, int16_t *out, size_t cnt);

    int64_t  nextNoteOn (PatternInternal &part, int64_t b, int64_t e);
    int64_t  nextNoteOff(int64_t b, int64_t e);

    void     noteOn (std::shared_ptr<PatternInternal> &pattern, PatternList::Note *r);
    void     noteOn (std::shared_ptr<PatternInternal> &pattern, int64_t time);
    void     noteOff(int64_t time);
    std::shared_ptr<PatternInternal> checkPattern(std::shared_ptr<PatternInternal> p);

    void     nextPattern();

    bool     hasVolumeCurves(PatternInternal &part, Instr &ins) const;
    void     volFromCurve(PatternInternal &part, Instr &ins, std::vector<float> &v);

    template<class T>
    bool     checkVariation(const T& item) const;
    int      getGroove() const;

    std::shared_ptr<Music::Internal>   current=nullptr;
    std::shared_ptr<Music::Internal>   nextMus=nullptr;
    std::atomic<DMUS_EMBELLISHT_TYPES> embellishment = {DMUS_EMBELLISHT_NORMAL};
    int64_t                            sampleCursor=0;

    std::shared_ptr<PatternInternal>   pattern=nullptr;
    int64_t                            patStart=0;
    int64_t                            patEnd  =0;
    std::atomic<uint32_t>              variationCounter={};
    std::atomic<size_t>                grooveCounter={};

    std::atomic<float>                 volume={1.f};
    std::vector<Active>                active;
    std::list<Instr>                   uniqInstr;
    std::vector<float>                 pcm, vol, pcmMix;
  };

}
