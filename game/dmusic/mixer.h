#pragma once

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>

#include <vector>
#include <cstdint>
#include <thread>
#include <atomic>
#include <list>
#include <mutex>
#include <unordered_set>
#include <limits>

#include "patternlist.h"
#include "music.h"
#include "reverb.h"
#ifdef OPENGOTHIC_WITH_NT5_DMSYNTH
#  include "nt5reverb.h"
#endif

namespace Dx8 {

class Mixer final {
  public:
    struct ChannelState final {
      uint32_t             pChannel=0;
      uint32_t             dwPatch=0;
      float                volume=0.f;
      float                pan=0.f;
      size_t               activeVoices=0;
      bool                 isMuted=false;
      bool                 isSolo=false;
      const DlsCollection* dls=nullptr;
      std::string          instrumentName;
      };

    struct ActivePatternState final {
      bool        hasPattern      = false;
      bool        hasPatternIndex = false;
      size_t      patternIndex    = 0;
      std::string patternName;
      };

    Mixer();
    ~Mixer();

    void     mix(int16_t *out, size_t samples);
    void     setVolume(float v);

    void     setMusic(const Music& m,DMUS_EMBELLISHT_TYPES embellishment=DMUS_EMBELLISHT_NORMAL);
    void     setMusicVolume(float v);
    int64_t  currentPlayTime() const;

    void     setChannelMuted(uint32_t pChannel, bool muted);
    void     setChannelSolo(uint32_t pChannel);
    void     clearChannelIsolation();
    void     getChannelStates(std::vector<ChannelState>& out) const;
    void     getActivePatternState(ActivePatternState& out) const;

  private:
    struct Instr;

    struct Active {
      int64_t           at=0;
      uint32_t          pChannel=0;
      uint8_t           note=0;
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
      float                     panLast=0.5f;  // CC10 state; 0=L, 0.5=C, 1=R
      float                     pitchBendLast=0.f; // DMUS_CURVET_PBCURVE state; -1..+1 (centre=0)
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
    uint64_t normalizeSegmentPos(uint64_t segmentPosMs) const;
    uint64_t timeToNextGrooveCommand(uint64_t segmentPosMs) const;
    int64_t  toSamplesScaled(uint64_t timeMs) const;
    double   tempoAt(uint64_t segmentPosMs) const;

    void     nextPattern();
    void     rebuildRuntimeWaves(const PatternInternal& patternData);
    void     selectPatternVariations(PatternInternal& patternData);
    bool     hasPlayableNotes(const PatternInternal& patternData) const;
    // Score the "richness" of a pattern under the *current* variation state
    // (count of notes plus weighted pan/volume/pitch-bend curves that pass
    // checkVariation). Used at theme-switch to pick the variation step that
    // activates the largest subset of the new segment's first pattern, so
    // panning/volume automation doesn't end up "eaten" by an unlucky
    // variation bitmask on the very first measure after the switch.
    uint32_t countActiveElements(const PatternInternal& patternData) const;
    // Brute-force the variationStep that maximises countActiveElements on the
    // chosen pattern and set variationCounter to it. Called only on
    // isThemeSwitch so normal pattern cycling keeps its monotonic counter.
    void     lockBestVariationForThemeSwitch(PatternInternal& patternData);
    size_t   pickPatternIndex(const std::vector<size_t>& candidates, DMUS_PATTERNT_TYPES mode, uint64_t commandKey);
    const PatternList::Groove* activeGrooveCommand(uint64_t segmentPosMs) const;

    bool     hasVolumeCurves(PatternInternal &part, Instr &ins) const;
    void     volFromCurve(PatternInternal &part, Instr &ins, std::vector<float> &v);
    bool     hasPanCurves(PatternInternal &part, Instr &ins) const;
    void     panFromCurve(PatternInternal &part, Instr &ins, std::vector<float> &v);
    bool     hasPitchBendCurves(PatternInternal &part, Instr &ins) const;
    float    pitchBendAt(PatternInternal &part, Instr &ins, int64_t sampleAbs);

    template<class T>
    bool     checkVariation(const T& item) const;
    int      getGroove() const;
    bool     isChannelMuted(uint32_t pChannel) const;

    std::shared_ptr<Music::Internal>   current=nullptr;
    std::shared_ptr<Music::Internal>   nextMus=nullptr;
    // True while the mixer is playing a one-shot bridge pattern (BREAK or
    // END) from the OLD style as a musical transition to the NEW segment.
    // During the bridge: nextMus is still queued but the swap is suppressed;
    // hardStop stays at patEnd (don't cut the bridge mid-measure); the
    // noteOff-on-switch cleanup is skipped (bridge blends with prev). The
    // flag clears at the subsequent nextPattern() call that finally swaps.
    bool                               transitionBridgeActive=false;
    std::atomic<DMUS_EMBELLISHT_TYPES> embellishment = {DMUS_EMBELLISHT_NORMAL};
    int64_t                            sampleCursor=0;

    std::shared_ptr<PatternInternal>   pattern=nullptr;
    std::vector<PatternList::Note>     runtimeWaves;
    int64_t                            patStart=0;
    int64_t                            patEnd  =0;
    uint64_t                           patDurationMs=0;
    uint64_t                           segmentCursorMs=0;
    double                             activeTempo=100.0;
    std::atomic<uint32_t>              variationCounter={};
    std::atomic<size_t>                grooveCounter={};
    uint32_t                           patternSelectionRng=0x9E3779B9u;
    uint64_t                           patternSelectionSignature=0;
    size_t                             patternSelectionStep=0;
    size_t                             patternSelectionOffset=0;
    size_t                             lastPatternIndex=std::numeric_limits<size_t>::max();
    std::vector<size_t>                patternRowBag;

    std::atomic<float>                 volume={1.f};
    // Reverb backend selection is a compile-time toggle: under
    // OPENGOTHIC_WITH_NT5_DMSYNTH we route the post-mix through the original
    // NT5 SVerb engine (ksWaves → Microsoft, used by DirectMusic on XP/2K),
    // otherwise we use the in-house FreeVerb (public-domain Schroeder).
    // Both classes expose init / setRoomSize / setDamping / setWidth / setWet
    // / setDry / setEnabled / mute / process, so call sites don't change.
#ifdef OPENGOTHIC_WITH_NT5_DMSYNTH
    NT5Reverb                          reverb_;
#else
    Reverb                             reverb_;
#endif
    std::vector<Active>                active;
    std::list<Instr>                   uniqInstr;
    std::unordered_set<uint32_t>       mutedChannels;
    int32_t                            soloChannel=-1;
    std::vector<float>                 pcm, vol, pan, pcmMix;

    // Persistent channel snapshot — updated on audio thread, read on main thread.
    // Preserves last known channels between notes so 'music list' always works.
    mutable std::mutex                 cacheMutex_;
    std::vector<ChannelState>          channelCache_;
  };

}
