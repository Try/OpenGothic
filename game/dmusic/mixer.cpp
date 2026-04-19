#include "mixer.h"

#include <Tempest/Application>
#include <Tempest/SoundEffect>
#include <Tempest/Sound>
#include <Tempest/Log>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>
#include <set>

#include "soundfont.h"
#include "wave.h"

using namespace Dx8;
using namespace Tempest;

constexpr float kPi = 3.14159265358979323846f;

static uint32_t nextRand(uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
  }

static int32_t randSymmetric(uint32_t& state, const uint32_t range) {
  if(range==0)
    return 0;
  const uint32_t span = range*2u + 1u;
  const uint32_t v    = nextRand(state) % span;
  return int32_t(v) - int32_t(range);
  }

static uint64_t hashCombine(uint64_t hash, uint64_t value) {
  hash ^= value + 0x9E3779B97F4A7C15ull + (hash<<6u) + (hash>>2u);
  return hash;
  }

Mixer::Mixer() {
  const size_t reserve=2048;
  pcm.reserve(reserve*2);
  pcmMix.reserve(reserve*2);
  vol.reserve(reserve);
  // uniqInstr.reserve(32);

  // Post-mix reverb. Tuned to approximate the I3DL2 "Music" preset used by
  // DirectMusic Producer's "Stereo + Reverb" output mode — this is the mode
  // Gothic's DLS bank was authored in, so without it the synth sounds "dry"
  // compared to the original game.
  reverb_.init(SoundFont::SampleRate);
#ifdef OPENGOTHIC_WITH_NT5_DMSYNTH
  // Exact NT5 dmsynth defaults (XP/2K DirectMusic), straight from
  //   multimedia/directx/dmusic/dmsynth/csynth.cpp, CSynth::CSynth():
  //     m_ReverbParams.fInGain          = 0.0;     //   0 dB in-gain
  //     m_ReverbParams.fReverbMix       = -10.0;   // -10 dB wet mix
  //     m_ReverbParams.fReverbTime      = 1000.0;  //   1.0 s RT60
  //     m_ReverbParams.fHighFreqRTRatio = 0.001;   // very dark tail
  // These are the values the original DirectMusic applied to every MIDI
  // port that didn't explicitly override DMUS_WAVES_REVERB_PARAMS, which
  // is the case for Gothic's segments — so this reproduces the reference
  // mix bit-for-bit (sverb engine + reference coefficients).
  reverb_.setParams(/*inGainDb=*/  0.f,
                    /*revMixDb=*/ -10.f,
                    /*revTimeMs=*/1000.f,
                    /*hfRatio=*/  0.001f);
  reverb_.setEnabled(true);
#else
  reverb_.setRoomSize(0.58f);   // ~1.5 s RT60 at 44.1 kHz; close to I3DL2 Music 1.49 s.
  reverb_.setDamping (0.45f);   // moderate HF absorption → warm tail, not metallic.
  reverb_.setWidth   (1.0f);
  reverb_.setWet     (0.22f);   // conservative send; avoids "swimmy" mix on percussion.
  reverb_.setDry     (1.0f);
  reverb_.setEnabled (true);
#endif
  }

Mixer::~Mixer() {
  for(auto& i:active)
    SoundFont::noteOff(i.ticket);
  }

void Mixer::setMusic(const Music& m,DMUS_EMBELLISHT_TYPES e) {
  if(current==m.impl)
    return;

  nextMus = m.impl;
  embellishment.store(e);
  // A new switch request cancels any half-consumed bridge state from the
  // previous switch (e.g. quickly toggling between zones).
  transitionBridgeActive = false;
  }

void Mixer::setMusicVolume(float v) {
  if(current)
    current->volume.store(v);
  if(nextMus)
    nextMus->volume.store(v);
  }

void Mixer::setChannelMuted(uint32_t pChannel, bool muted) {
  soloChannel = -1;
  if(muted)
    mutedChannels.insert(pChannel);
  else
    mutedChannels.erase(pChannel);
  }

void Mixer::setChannelSolo(uint32_t pChannel) {
  mutedChannels.clear();
  soloChannel = int32_t(pChannel);
  }

void Mixer::clearChannelIsolation() {
  mutedChannels.clear();
  soloChannel = -1;
  }

bool Mixer::isChannelMuted(uint32_t pChannel) const {
  if(soloChannel>=0 && pChannel!=uint32_t(soloChannel))
    return true;
  return mutedChannels.find(pChannel)!=mutedChannels.end();
  }

void Mixer::getChannelStates(std::vector<ChannelState>& out) const {
  // Read from the persistent cache populated by the audio thread.
  // This survives gaps between note-on/note-off so 'music list' always works.
  std::lock_guard<std::mutex> lk(cacheMutex_);
  out = channelCache_;
  // Refresh muted/solo flags from current state (they change via setChannelMuted etc.)
  for(auto& s : out) {
    s.isMuted = isChannelMuted(s.pChannel);
    s.isSolo  = (soloChannel>=0 && s.pChannel==uint32_t(soloChannel));
    }
  }

void Mixer::getActivePatternState(ActivePatternState& out) const {
  out = ActivePatternState{};
  if(pattern==nullptr)
    return;

  out.hasPattern = true;
  out.patternName = pattern->name;
  if(lastPatternIndex!=std::numeric_limits<size_t>::max()) {
    out.hasPatternIndex = true;
    out.patternIndex = lastPatternIndex;
    }
  }

int64_t Mixer::currentPlayTime() const {
  return (sampleCursor*1000/SoundFont::SampleRate);
  }

uint64_t Mixer::normalizeSegmentPos(uint64_t segmentPosMs) const {
  const auto mus = current;
  if(mus==nullptr)
    return segmentPosMs;

  if(mus->loopEnd>mus->loopStart) {
    if(segmentPosMs<mus->loopEnd)
      return segmentPosMs;
    const uint64_t span = mus->loopEnd-mus->loopStart;
    if(span>0u)
      return mus->loopStart + (segmentPosMs-mus->loopStart)%span;
    }
  if(mus->timeTotal>0u)
    return segmentPosMs%mus->timeTotal;
  return segmentPosMs;
  }

double Mixer::tempoAt(uint64_t segmentPosMs) const {
  const auto mus = current;
  if(mus==nullptr)
    return 100.0;

  segmentPosMs = normalizeSegmentPos(segmentPosMs);

  // Step-lookup is intentional: DirectMusic's DMUS_IO_TEMPO_ITEM carries
  // only {time, tempo} — no interpolation flag — and the reference
  // performance holds each tempo constant until the next event. Tempo
  // curves in DM are a *different* mechanism (not present in Gothic
  // style data), so there is no ramp to interpolate here.
  double tempo = mus->baseTempo>0.0 ? mus->baseTempo : 100.0;
  for(const auto& e : mus->tempo) {
    if(e.at>segmentPosMs)
      break;
    if(e.tempo>0.0)
      tempo = e.tempo;
    }
  return tempo;
  }

int64_t Mixer::toSamplesScaled(uint64_t timeMs) const {
  const auto mus = current;
  const double baseTempo = (mus!=nullptr && mus->baseTempo>0.0) ? mus->baseTempo : 100.0;
  const double tempo = (activeTempo>0.0) ? activeTempo : baseTempo;
  const double scale = baseTempo/tempo;
  return int64_t(std::llround(double(timeMs)*double(SoundFont::SampleRate)*scale/1000.0));
  }

int64_t Mixer::nextNoteOn(PatternList::PatternInternal& part,int64_t b,int64_t e) {
  int64_t nextDt    = std::numeric_limits<int64_t>::max();
  int64_t timeTotal = toSamplesScaled(part.timeTotal);
  bool    inv       = (e<b);

  b-=patStart;
  e-=patStart;

  for(auto& i:runtimeWaves) {
    int64_t at = toSamplesScaled(i.at);
    if((b<at && at<=e)^inv) {
      int64_t t = at-b;
      if(inv)
        t += timeTotal;
      if(t<nextDt && i.duration>0)
        nextDt = t;
      }
    }

  return nextDt;
  }

int64_t Mixer::nextNoteOff(int64_t b, int64_t /*e*/) {
  int64_t actT = std::numeric_limits<int64_t>::max();

  for(auto& i:active) {
    int64_t at = i.at;
    int64_t dt = at>b ? at-b : 0;
    if(dt<actT)
      actT = dt;
    }
  return actT;
  }

void Mixer::noteOn(std::shared_ptr<PatternInternal>& inPattern, PatternList::Note *r) {
  if(!checkVariation(*r))
    return;

  // Send MIDI CCs before the note-on so backends that honour them apply
  // the values immediately. No-op for the TSF backend.
  // CC1 = mod wheel = 0 (no vibrato by default; let the music data drive it).
  // CC7 = channel volume scaled from instrument volume (linear → 0..127).
  // CC11 = expression at full (127).
  {
  auto& fnt = r->inst->font;
  fnt.setCC(1,  0);
  fnt.setCC(7,  std::min(127, int(r->inst->volume * 127.f + 0.5f)));
  fnt.setCC(11, 127);
  }

  Active a;
  a.at       = sampleCursor + toSamplesScaled(r->duration);
  a.note     = r->note;
  a.pChannel = (r->inst!=nullptr ? r->inst->key : 0u);
  a.ticket   = r->inst->font.noteOn(r->note,r->velosity);
  if(a.ticket==nullptr)
    return;

  for(auto& i:uniqInstr)
    if(i.ptr==r->inst) {
      a.parent = &i;
      a.parent->counter++;
      active.push_back(a);
      return;
      }
  Instr u;
  u.ptr     = r->inst;
  u.pattern = inPattern;
  uniqInstr.push_back(u);

  a.parent = &uniqInstr.back();
  a.parent->counter++;

  active.push_back(a);
  }

void Mixer::noteOn(std::shared_ptr<PatternInternal>& inPattern, int64_t time) {
  time-=patStart;

  size_t n=0;
  for(auto& i:runtimeWaves) {
    int64_t at = toSamplesScaled(i.at);
    if(at==time) {
      noteOn(inPattern,&i);
      ++n;
      }
    }
  if(n==0)
    throw std::runtime_error("mixer critical error");
  }

void Mixer::noteOff(int64_t time) {
  size_t sz=0;
  for(size_t i=0;i<active.size();++i){
    if(active[i].at>time) {
      active[sz]=active[i];
      sz++;
      } else {
      SoundFont::noteOff(active[i].ticket);
      active[i].parent->counter--;
      }
    }
  active.resize(sz);
  }

const PatternList::Groove* Mixer::activeGrooveCommand(uint64_t segmentPosMs) const {
  const auto mus = current;
  if(mus==nullptr || mus->groove.empty())
    return nullptr;

  const uint64_t pos = normalizeSegmentPos(segmentPosMs);

  const PatternList::Groove* first = nullptr;
  const PatternList::Groove* currentGroove = nullptr;
  for(const auto& g : mus->groove) {
    if(first==nullptr)
      first = &g;
    if(g.at>pos)
      break;
    currentGroove = &g;
    }

  if(currentGroove!=nullptr)
    return currentGroove;
  return first;
  }

uint64_t Mixer::timeToNextGrooveCommand(uint64_t segmentPosMs) const {
  const auto mus = current;
  if(mus==nullptr || mus->groove.empty())
    return std::numeric_limits<uint64_t>::max();

  const uint64_t pos = normalizeSegmentPos(segmentPosMs);

  auto inActiveLoopWindow = [&](uint64_t at) {
    if(mus->loopEnd>mus->loopStart)
      return at>=mus->loopStart && at<mus->loopEnd;
    if(mus->timeTotal>0u)
      return at<mus->timeTotal;
    return true;
    };

  uint64_t cycleWrapAt = 0;
  uint64_t cycleResumeAt = 0;
  if(mus->loopEnd>mus->loopStart) {
    cycleWrapAt = mus->loopEnd;
    cycleResumeAt = mus->loopStart;
    }
  else if(mus->timeTotal>0u) {
    cycleWrapAt = mus->timeTotal;
    cycleResumeAt = 0u;
    }

  for(const auto& g : mus->groove) {
    if(g.at<=pos)
      continue;
    if(cycleWrapAt>0u && g.at>=cycleWrapAt)
      break;
    return g.at-pos;
    }

  if(cycleWrapAt==0u || cycleWrapAt<=pos)
    return std::numeric_limits<uint64_t>::max();

  for(const auto& g : mus->groove) {
    if(!inActiveLoopWindow(g.at))
      continue;
    if(g.at<cycleResumeAt)
      continue;
    return (cycleWrapAt-pos) + (g.at-cycleResumeAt);
    }

  return std::numeric_limits<uint64_t>::max();
  }

size_t Mixer::pickPatternIndex(const std::vector<size_t>& candidates, DMUS_PATTERNT_TYPES mode, uint64_t commandKey) {
  if(candidates.empty())
    return std::numeric_limits<size_t>::max();
  if(candidates.size()==1)
    return candidates[0];

  uint64_t signature = hashCombine(uint64_t(mode),commandKey);
  signature = hashCombine(signature, candidates.size());
  for(auto idx : candidates)
    signature = hashCombine(signature, idx+1u);

  if(patternSelectionSignature!=signature) {
    patternSelectionSignature = signature;
    patternSelectionStep      = 0;
    patternSelectionOffset    = 0;
    patternRowBag.clear();
    }

  auto randomCandidate = [&]() -> size_t {
    const size_t pos = size_t(nextRand(patternSelectionRng) % uint32_t(candidates.size()));
    return candidates[pos];
    };

  switch(mode) {
    case DMUS_PATTERNT_REPEAT: {
      for(auto idx : candidates)
        if(idx==lastPatternIndex)
          return idx;
      return candidates[0];
      }

    case DMUS_PATTERNT_SEQUENTIAL: {
      const size_t pos = patternSelectionStep % candidates.size();
      patternSelectionStep++;
      return candidates[pos];
      }

    case DMUS_PATTERNT_RANDOM_START: {
      if(patternSelectionStep==0)
        patternSelectionOffset = size_t(nextRand(patternSelectionRng) % uint32_t(candidates.size()));
      const size_t pos = (patternSelectionOffset + patternSelectionStep) % candidates.size();
      patternSelectionStep++;
      return candidates[pos];
      }

    case DMUS_PATTERNT_NO_REPEAT: {
      if(candidates.size()==2)
        return (candidates[0]==lastPatternIndex ? candidates[1] : candidates[0]);
      for(size_t i=0; i<8; ++i) {
        const size_t idx = randomCandidate();
        if(idx!=lastPatternIndex)
          return idx;
        }
      for(auto idx : candidates)
        if(idx!=lastPatternIndex)
          return idx;
      return candidates[0];
      }

    case DMUS_PATTERNT_RANDOM_ROW: {
      if(patternRowBag.empty())
        patternRowBag = candidates;
      const size_t pick = size_t(nextRand(patternSelectionRng) % uint32_t(patternRowBag.size()));
      const size_t idx = patternRowBag[pick];
      patternRowBag[pick] = patternRowBag.back();
      patternRowBag.pop_back();
      return idx;
      }

    case DMUS_PATTERNT_RANDOM:
    default:
      return randomCandidate();
    }
  }

void Mixer::nextPattern() {
  auto mus = current;
  if(mus==nullptr || mus->pptn.empty()) {
    // no active music
    pattern = nullptr;
    patDurationMs = 0;
    return;
    }

  auto prev = pattern;
  bool interruptedByGrooveBoundary = false;
  if(prev!=nullptr && nextMus==nullptr && sampleCursor>=patEnd) {
    interruptedByGrooveBoundary = (patDurationMs>0u && patDurationMs<prev->timeTotal);
    segmentCursorMs += patDurationMs;
    if(current)
      segmentCursorMs = normalizeSegmentPos(segmentCursorMs);
    }

  pattern = nullptr;
  const DMUS_EMBELLISHT_TYPES em = embellishment.exchange(DMUS_EMBELLISHT_NORMAL);

  // Pre-compute groove level for the OLD style BEFORE the bridge decision.
  // This is also used later for the actual pattern pick — recomputing it
  // after the swap would bind it to the NEW style, which we don't want:
  // bridge candidates are filtered by OLD-style grooves, only post-swap
  // candidates use the NEW style's grooves.
  //
  // We compute it twice in total (once here pre-swap for bridge gating,
  // once below post-swap for the actual pick) because the post-swap
  // `mus` may point at a different groove list. For the no-swap path
  // (no theme change) both computations yield the same `groove` value.
  const PatternList::Groove* preCommand = activeGrooveCommand(segmentCursorMs);
  int preGroove = preCommand!=nullptr ? int(preCommand->bGrooveLevel) : getGroove();
  if(preCommand!=nullptr && preCommand->bGrooveRange>0u) {
    const uint32_t radius = uint32_t(preCommand->bGrooveRange/2u);
    if(radius>0u) {
      const uint64_t commandKeyPre = preCommand->at;
      uint64_t grooveSeed = hashCombine(commandKeyPre, uint64_t(grooveCounter.load()+1u));
      grooveSeed = hashCombine(grooveSeed, uint64_t(lastPatternIndex+1u));
      uint32_t grooveRng = uint32_t(grooveSeed ^ (grooveSeed>>32u));
      if(grooveRng==0u)
        grooveRng = 0x7F4A7C15u;
      preGroove += randSymmetric(grooveRng, radius);
      preGroove = std::clamp(preGroove, 0, 100);
      }
    }

  // Transition bridge decision — before the swap. If the request is a
  // NORMAL theme-change AND the OLD style has a BREAK (preferred) or END
  // pattern WHOSE GROOVE BUCKET MATCHES the current groove, play that as
  // a one-shot bridge and defer the swap until the bridge finishes. This
  // mirrors the DirectMusic reference's transitionTemplate behaviour (play
  // final phrase of old style, then open new). Skipped for explicit
  // embellishment requests (combat) and when we've already played the
  // bridge for THIS switch.
  //
  // Groove-gating matters: without it, a BREAK pattern authored for a
  // different groove band would satisfy `hasEmb` but produce zero
  // candidates in `buildCandidates` below, and `pattern` would end up
  // null — the early return at the empty-runtimeWaves branch would
  // then leave us stuck with `transitionBridgeActive=true` and `nextMus`
  // unconsumed.
  DMUS_EMBELLISHT_TYPES bridgeEmb = DMUS_EMBELLISHT_NORMAL;
  bool playBridge = false;
  if(nextMus!=nullptr && em==DMUS_EMBELLISHT_NORMAL && !transitionBridgeActive) {
    auto hasEmb = [&](DMUS_EMBELLISHT_TYPES want) {
      for(const auto& p : mus->pptn) {
        if(p->ptnh.wEmbellishment!=want || p->timeTotal==0)
          continue;
        if(mus->groove.size()==0)
          return true;
        if(p->ptnh.bGrooveBottom<=preGroove && preGroove<=p->ptnh.bGrooveTop)
          return true;
        }
      return false;
      };
    if(hasEmb(DMUS_EMBELLISHT_BREAK)) {
      playBridge = true;
      bridgeEmb  = DMUS_EMBELLISHT_BREAK;
      }
    else if(hasEmb(DMUS_EMBELLISHT_END)) {
      playBridge = true;
      bridgeEmb  = DMUS_EMBELLISHT_END;
      }
    }

  // Actual theme swap only happens once the bridge has played (or if there
  // is no bridge available). `isThemeSwitch` drives downstream cleanup
  // (variation lock, noteOff) so it must be false while the bridge plays —
  // otherwise the bridge itself would kill its own predecessor's tails.
  // NOT const: if the bridge turns out to have no playable candidates
  // (belt-and-suspenders for edge cases not caught by groove-gating in
  // hasEmb above — e.g. a future change that gates on more state), we
  // fall through to a real theme-switch below and flip this.
  bool isThemeSwitch = (nextMus!=nullptr && !playBridge);

  // Performs the actual style swap (OLD → NEW). Factored out so the
  // bridge-empty fallback below can re-use it without duplication.
  auto performThemeSwap = [&]() {
    current = nextMus;
    nextMus = nullptr;
    mus     = current;
    prev    = nullptr;
    segmentCursorMs = 0;
    activeTempo = tempoAt(segmentCursorMs);
    grooveCounter.store(0);
    // Intentionally do NOT reset variationCounter here: DirectMusic sequential
    // variation instruments pick `activeVariation = variationStep % dwVarCount`.
    // Forcing it to 0 on theme-switch means every such instrument collapses to
    // its 0-th variation, and any note whose `dwVariation` bitmask lacks bit 0
    // is silently dropped on the first pattern of the new segment. Instead
    // the post-swap `lockBestVariationForThemeSwitch()` below brute-forces the
    // variationStep that activates the richest subset of notes+curves.
    patternSelectionSignature = 0;
    patternSelectionStep      = 0;
    patternSelectionOffset    = 0;
    patternRowBag.clear();
    lastPatternIndex = std::numeric_limits<size_t>::max();
    patternSelectionRng = uint32_t((reinterpret_cast<uintptr_t>(mus.get())>>4u) ^ 0x9E3779B9u);
    if(patternSelectionRng==0)
      patternSelectionRng = 0xA341316Cu;
    transitionBridgeActive = false;  // bridge (if any) consumed
    };

  if(playBridge) {
    transitionBridgeActive = true;
    // keep `current`, `nextMus`, segmentCursorMs unchanged — the bridge is
    // a continuation of the old segment's musical flow.
    }
  else if(isThemeSwitch) {
    performThemeSwap();
    }

  // Effective embellishment this call:
  //   • bridge       → BREAK/END from OLD style
  //   • theme-switch → INTRO from NEW style (falls back to NORMAL below)
  //   • otherwise    → whatever was explicitly requested (usually NORMAL)
  DMUS_EMBELLISHT_TYPES emEff = em;
  if(playBridge)
    emEff = bridgeEmb;
  else if(isThemeSwitch && em==DMUS_EMBELLISHT_NORMAL)
    emEff = DMUS_EMBELLISHT_INTRO;

  // NOT const: the bridge-empty fallback below may recompute these
  // against the post-swap (NEW) style.
  const PatternList::Groove* command = activeGrooveCommand(segmentCursorMs);
  DMUS_PATTERNT_TYPES repeatMode = DMUS_PATTERNT_NO_REPEAT;
  if(command!=nullptr && command->repeatMode<=DMUS_PATTERNT_RANDOM_ROW)
    repeatMode = command->repeatMode;
  uint64_t commandKey = command!=nullptr ? command->at : segmentCursorMs;

  int groove = command!=nullptr ? int(command->bGrooveLevel) : getGroove();
  if(command!=nullptr && command->bGrooveRange>0u) {
    // DirectMusic command range is symmetric around groove level.
    const uint32_t radius = uint32_t(command->bGrooveRange/2u);
    if(radius>0u) {
      uint64_t grooveSeed = hashCombine(commandKey, uint64_t(grooveCounter.load()+1u));
      grooveSeed = hashCombine(grooveSeed, uint64_t(lastPatternIndex+1u));
      uint32_t grooveRng = uint32_t(grooveSeed ^ (grooveSeed>>32u));
      if(grooveRng==0u)
        grooveRng = 0x7F4A7C15u;
      groove += randSymmetric(grooveRng, radius);
      groove = std::clamp(groove, 0, 100);
      }
    }

  auto matchPattern = [&](DMUS_EMBELLISHT_TYPES e, const PatternInternal& ptr) {
    if(ptr.timeTotal==0)
      return false;
    // Exact embellishment match: NORMAL patterns are invisible to BREAK
    // queries etc., and MOTIF patterns are invisible to NORMAL (they're
    // one-shot stingers played ON TOP of the current segment in real DM;
    // Gothic's setMusic never requests MOTIF, so these patterns stay
    // dormant — which matches the game's behaviour on the reference
    // engine where no script triggers a motif).
    if(ptr.ptnh.wEmbellishment!=e)
      return false;
    if(mus->groove.size()==0)
      return true;
    return ptr.ptnh.bGrooveBottom<=groove && groove<=ptr.ptnh.bGrooveTop;
    };

  auto buildCandidates = [&](DMUS_EMBELLISHT_TYPES e, std::vector<size_t>& out) {
    out.clear();
    for(size_t i=0;i<mus->pptn.size();++i) {
      if(!matchPattern(e, *mus->pptn[i]))
        continue;
      out.push_back(i);
      }
    };

  std::vector<size_t> candidates;
  candidates.reserve(mus->pptn.size());
  buildCandidates(emEff, candidates);

  // Belt-and-suspenders: if we decided to play a bridge but buildCandidates
  // produced nothing (e.g. a future divergence between hasEmb above and
  // matchPattern here — different groove logic, a new gate, etc.), tear
  // down the bridge and fall through to the actual theme-swap. Without
  // this the mixer would get stuck with transitionBridgeActive=true and
  // nextMus unconsumed.
  if(candidates.empty() && playBridge) {
    playBridge              = false;
    transitionBridgeActive  = false;
    isThemeSwitch           = (nextMus!=nullptr);
    if(isThemeSwitch)
      performThemeSwap();
    // Recompute groove, command, and matchPattern for the NEW style.
    command    = activeGrooveCommand(segmentCursorMs);
    repeatMode = DMUS_PATTERNT_NO_REPEAT;
    if(command!=nullptr && command->repeatMode<=DMUS_PATTERNT_RANDOM_ROW)
      repeatMode = command->repeatMode;
    commandKey = command!=nullptr ? command->at : segmentCursorMs;
    groove = command!=nullptr ? int(command->bGrooveLevel) : getGroove();
    if(command!=nullptr && command->bGrooveRange>0u) {
      const uint32_t radius = uint32_t(command->bGrooveRange/2u);
      if(radius>0u) {
        uint64_t grooveSeed = hashCombine(commandKey, uint64_t(grooveCounter.load()+1u));
        grooveSeed = hashCombine(grooveSeed, uint64_t(lastPatternIndex+1u));
        uint32_t grooveRng = uint32_t(grooveSeed ^ (grooveSeed>>32u));
        if(grooveRng==0u)
          grooveRng = 0x7F4A7C15u;
        groove += randSymmetric(grooveRng, radius);
        groove = std::clamp(groove, 0, 100);
        }
      }
    emEff = (isThemeSwitch && em==DMUS_EMBELLISHT_NORMAL) ? DMUS_EMBELLISHT_INTRO : em;
    buildCandidates(emEff, candidates);
    }

  // If INTRO was requested on theme-switch but the new segment has no INTRO patterns,
  // fall back silently to NORMAL — avoids a gap/silence at the start.
  if(candidates.empty() && emEff==DMUS_EMBELLISHT_INTRO) {
    emEff = DMUS_EMBELLISHT_NORMAL;
    buildCandidates(emEff, candidates);
    }

  const size_t pickedIndex = pickPatternIndex(candidates, repeatMode, commandKey);
  std::vector<size_t> ordered;
  ordered.reserve(candidates.size());
  if(pickedIndex!=std::numeric_limits<size_t>::max())
    ordered.push_back(pickedIndex);
  for(size_t idx : candidates)
    if(idx!=pickedIndex)
      ordered.push_back(idx);

  std::shared_ptr<PatternInternal> fallbackPattern = nullptr;
  size_t fallbackIndex = std::numeric_limits<size_t>::max();
  bool foundPlayable = false;
  for(size_t idx : ordered) {
    auto& ptr = mus->pptn[idx];
    pattern = std::shared_ptr<PatternList::PatternInternal>(mus,ptr.get());
    if(fallbackPattern==nullptr) {
      fallbackPattern = pattern;
      fallbackIndex   = idx;
      }

    rebuildRuntimeWaves(*pattern);
    selectPatternVariations(*pattern);
    if(hasPlayableNotes(*pattern)) {
      foundPlayable = true;
      lastPatternIndex = idx;
      break;
      }
    }

  if(!foundPlayable && fallbackPattern!=nullptr) {
    pattern = fallbackPattern;
    rebuildRuntimeWaves(*pattern);
    selectPatternVariations(*pattern);
    lastPatternIndex = fallbackIndex;
    }

  if(pattern==nullptr || runtimeWaves.empty()) {
    runtimeWaves.clear();
    patDurationMs = 0;
    return;
    }

  // On a theme-switch, sweep possible variationStep values and lock the one
  // that activates the richest subset of notes+pan/vol/bend curves in the
  // first pattern of the new segment. Without this, inheriting the previous
  // segment's counter happened to pick a variation whose bitmask matched
  // only a few notes — audible as "channels drop" and "panning eaten" on
  // the very first measure after the switch, then recovered on subsequent
  // pattern loops as the counter ticked past the unlucky value.
  if(isThemeSwitch)
    lockBestVariationForThemeSwitch(*pattern);

  // Inject pre-roll only across pattern boundaries.
  const bool injectPreRoll = (prev!=nullptr && prev.get()!=pattern.get());

  if(pattern->timeTotal>0) {
    grooveCounter.fetch_add(1);
    }

  // Note-off cleanup policy. The bridge (OLD-style BREAK/END before a NORMAL
  // theme switch) is treated as a *continuation* of the previous pattern —
  // its notes should blend with the old tail, not wipe it — so we suppress
  // the cleanup here when `playBridge` is true even though emEff!=NORMAL.
  if(!playBridge &&
     (isThemeSwitch || emEff!=DMUS_EMBELLISHT_NORMAL || interruptedByGrooveBoundary)) {
    // Clean-slate note-off whenever the pattern context is discontinuous:
    //   • theme switch — we're entering a new segment entirely; the old
    //     theme's still-ringing notes must not bleed into the new one.
    //   • non-NORMAL embellishment requested by the game (FILL/BREAK/END/
    //     INTRO from .zen transtype) — explicit pattern transition.
    //   • groove-boundary interruption — mid-measure groove change cut the
    //     previous pattern short; stop its carried notes to avoid stacking.
    // Pass INT64_MAX so every entry in `active` is released in one pass
    // (the per-voice release envelope still runs — this is a note-off,
    // not a hard voice kill, so short DLS tails decay naturally).
    noteOff(std::numeric_limits<int64_t>::max());
    }

  patStart = sampleCursor;
  activeTempo = tempoAt(segmentCursorMs);
  patDurationMs = pattern->timeTotal;
  const uint64_t untilCommandMs = timeToNextGrooveCommand(segmentCursorMs);
  if(untilCommandMs>0u && untilCommandMs<patDurationMs)
    patDurationMs = untilCommandMs;
  patEnd = patStart+toSamplesScaled(patDurationMs);

  auto extendActiveSustain = [&](const PatternList::Note& rec) -> bool {
    const uint32_t pChannel = (rec.inst!=nullptr ? rec.inst->key : 0u);
    const int64_t desiredOff = sampleCursor + toSamplesScaled(rec.duration);
    for(auto& a : active) {
      if(a.pChannel!=pChannel || a.note!=rec.note)
        continue;
      if(a.at<=sampleCursor)
        continue;
      if(a.at<desiredOff)
        a.at = desiredOff;
      return true;
      }
    return false;
    };

  for(auto& i:runtimeWaves)
    if(i.at==0) {
      PatternList::Note rec = i;
      if(rec.preRoll>0) {
        if(rec.duration<=rec.preRoll)
          continue;
        rec.duration -= rec.preRoll;
        if(extendActiveSustain(rec))
          continue;
        }
      noteOn(pattern,&rec);
      }
  if(injectPreRoll) {
    // Large negative offsets can represent full-bar anticipations and may sound
    // like repeated section starts if injected at boundary. Keep only local
    // anticipations and avoid retriggering notes that are already sustaining.
    double beatMs = 600.0;
    if(activeTempo>0.0)
      beatMs = (60000.0/activeTempo) * (4.0/std::max<int>(1,int(pattern->styh.timeSig.bBeat)));
    const uint64_t maxInjectedPreRollMs = uint64_t(std::llround(std::clamp(beatMs*1.25, 200.0, 1200.0)));

    for(auto& i:runtimeWaves) {
      if(i.preRoll==0 || i.duration<=i.preRoll)
        continue;
      if(i.at==0)
        continue;
      PatternList::Note pre = i;
      pre.at       = 0;
      pre.duration = i.duration - i.preRoll;

      if(extendActiveSustain(pre))
        continue;

      if(i.preRoll>maxInjectedPreRollMs)
        continue;

      noteOn(pattern,&pre);
      }
    }
  variationCounter.fetch_add(1);
  }

void Mixer::rebuildRuntimeWaves(const PatternInternal& patternData) {
  runtimeWaves = patternData.waves;
  if(runtimeWaves.empty() || patternData.timeTotal==0)
    return;

  static constexpr bool kEnableLegacyNoteRandomization = true;
  if(!kEnableLegacyNoteRandomization)
    return;

  uint32_t seed = uint32_t((variationCounter.load()+1u)*1664525u + 1013904223u);
  seed ^= uint32_t(grooveCounter.load()*2246822519u);
  seed ^= uint32_t(reinterpret_cast<uintptr_t>(&patternData) >> 4u);
  if(seed==0)
    seed = 0xA3C59AC3u;

  const int64_t patternTimeTotal = int64_t(patternData.timeTotal);
  for(auto& i:runtimeWaves) {
    if((i.noteFlags & DMUS_NOTEF_REGENERATE)==0)
      continue;

    int64_t rawStart = (i.preRoll>0) ? -int64_t(i.preRoll) : int64_t(i.at);
    rawStart += int64_t(randSymmetric(seed, i.timeRange));
    int64_t dur = int64_t(i.duration) + int64_t(randSymmetric(seed, i.durRange));
    int32_t vel = int32_t(i.velosity) + randSymmetric(seed, i.velRange);

    if(dur<1)
      dur = 1;
    if(dur>patternTimeTotal)
      dur = patternTimeTotal;
    if(vel<0)
      vel = 0;
    if(vel>127)
      vel = 127;

    int64_t at = rawStart;
    at %= patternTimeTotal;
    if(at<0)
      at += patternTimeTotal;

    i.at       = uint64_t(at);
    i.preRoll  = rawStart<0 ? uint64_t(-rawStart) : 0;
    i.duration = uint64_t(dur);
    i.velosity = uint8_t(vel);
    }

  std::sort(runtimeWaves.begin(),runtimeWaves.end(),[](const PatternList::Note& a, const PatternList::Note& b) {
    return a.at<b.at;
    });
  }

void Mixer::selectPatternVariations(PatternInternal& patternData) {
  const uint32_t variationStep = variationCounter.load();
  std::array<int32_t, 256> lockedVariation;
  lockedVariation.fill(std::numeric_limits<int32_t>::min());
  uint32_t rng = uint32_t((variationStep+1u)*1664525u + 1013904223u);

  auto selectBaseVariation = [&](const PatternList::InsInternal& inst) -> int32_t {
    switch(inst.randomVariation) {
      case 1: // random
      case 3: // no-repeat/random-row (legacy fallback)
      case 4: // defensive: unknown values are treated like random
      case 5:
        return int32_t(nextRand(rng));
      case 0: // sequential
      case 2: // random-start (legacy fallback: sequential)
      default:
        return int32_t(variationStep);
      }
    };

  for(auto& inst : patternData.instruments) {
    if(inst.dwVarCount==0) {
      inst.activeVariation = 0;
      continue;
      }

    int32_t baseVariation = 0;
    if(inst.variationLockId==0) {
      baseVariation = selectBaseVariation(inst);
      }
    else {
      int32_t& locked = lockedVariation[inst.variationLockId];
      if(locked==std::numeric_limits<int32_t>::min())
        locked = selectBaseVariation(inst);
      baseVariation = locked;
      }

    const uint32_t varPos = uint32_t(baseVariation) % std::max<uint32_t>(1u,inst.dwVarCount);
    inst.activeVariation = inst.variationIds[varPos];
    }
  }

bool Mixer::hasPlayableNotes(const PatternInternal& /*patternData*/) const {
  for(const auto& note : runtimeWaves) {
    if(note.inst==nullptr || note.duration==0)
      continue;
    if(note.inst->dwVarCount==0 || note.dwVariation==0u)
      return true;
    const uint32_t vbit = note.inst->activeVariation;
    if(vbit<32u && (note.dwVariation & (1u<<vbit))!=0u)
      return true;
    }
  return false;
  }

uint32_t Mixer::countActiveElements(const PatternInternal& patternData) const {
  // Notes that pass the current variation gate in runtimeWaves.
  uint32_t score = 0;
  for(const auto& note : runtimeWaves) {
    if(note.inst==nullptr || note.duration==0)
      continue;
    if(note.inst->dwVarCount==0 || note.dwVariation==0u) {
      ++score;
      continue;
      }
    const uint32_t vbit = note.inst->activeVariation;
    if(vbit<32u && (note.dwVariation & (1u<<vbit))!=0u)
      ++score;
    }

  // Curves get a small multiplier: a single pan/volume curve carries more
  // perceptual weight than a single note (it shapes the whole instrument
  // for a measure). Keeps "panning eaten" from being masked by a pattern
  // that has dozens of accompaniment notes but one critical pan curve.
  auto scoreCurves = [&](const std::vector<PatternList::Curve>& curves) {
    uint32_t s = 0;
    for(const auto& c : curves) {
      if(c.inst==nullptr)
        continue;
      if(c.inst->dwVarCount==0 || c.dwVariation==0u) {
        s += 4;
        continue;
        }
      const uint32_t vbit = c.inst->activeVariation;
      if(vbit<32u && (c.dwVariation & (1u<<vbit))!=0u)
        s += 4;
      }
    return s;
    };
  score += scoreCurves(patternData.volume);
  score += scoreCurves(patternData.pan);
  score += scoreCurves(patternData.pitchBend);
  return score;
  }

void Mixer::lockBestVariationForThemeSwitch(PatternInternal& patternData) {
  // Probe a window of candidate variationStep values and keep the one that
  // activates the largest set of notes+curves. 32 steps always covers any
  // valid dwVarCount (the bitmask itself is 32-bit) and is cheap to brute-
  // force (≈ linear in runtimeWaves size * 32). We intentionally do NOT
  // touch variationCounter for non-theme-switch calls: inside a segment
  // the sequential counter is how DirectMusic intends patterns to cycle.
  uint32_t bestStep  = variationCounter.load();
  uint32_t bestScore = 0;
  {
    // Baseline: whatever selectPatternVariations already applied.
    bestScore = countActiveElements(patternData);
    }
  for(uint32_t step = 0; step < 32u; ++step) {
    variationCounter.store(step);
    selectPatternVariations(patternData);
    const uint32_t s = countActiveElements(patternData);
    if(s>bestScore) {
      bestScore = s;
      bestStep  = step;
      }
    }
  variationCounter.store(bestStep);
  selectPatternVariations(patternData);
  }

Mixer::Step Mixer::stepInc(PatternInternal& pptn, int64_t b, int64_t e, int64_t samplesRemain) {
  int64_t nextT   = nextNoteOn (pptn,b,e);
  int64_t offT    = nextNoteOff(b,e);
  int64_t samples = std::min(offT,nextT);

  Step s;
  if(samples>samplesRemain) {
    s.samples=samplesRemain;
    return s;
    }

  s.nextOn  = nextT;//nextNoteOn (pptn,b,e);
  s.nextOff = offT;//nextNoteOff(b,e);
  s.samples = samples;
  return s;
  }

void Mixer::stepApply(std::shared_ptr<PatternInternal> &pptn, const Mixer::Step &s,int64_t b) {
  if(s.nextOff<s.nextOn) {
    noteOff(s.nextOff+b);
    } else
  if(s.nextOff>s.nextOn) {
    noteOn (pptn,s.nextOn+b);
    } else
  if(s.nextOff==s.nextOn) {
    noteOff(s.nextOff+b);
    noteOn (pptn,s.nextOn+b);
    }
  }

std::shared_ptr<Mixer::PatternInternal> Mixer::checkPattern(std::shared_ptr<PatternInternal> p) {
  auto cur = current;
  if(cur==nullptr)
    return nullptr;

  if(p==nullptr) {
    grooveCounter.store(0);
    } else {
    for(auto& i:cur->pptn)
      if(i.get()==p.get()) {
        return p;
        }
    }
  // null or foreign
  nextPattern();
  return pattern;
  }

void Mixer::mix(int16_t *out, size_t samples) {
  std::memset(out,0,2*samples*sizeof(int16_t));

  auto cur = current;
  if(cur==nullptr) {
    current = nextMus;
    return;
    }

  if(cur->pptn.empty()) {
    current = nextMus;
    return;
    }

  auto pat = checkPattern(pattern);

  size_t samplesRemain = samples;
  while(samplesRemain>0) {
    if(pat==nullptr)
      break;

    // Measure-aligned switch deadline for NORMAL-embellishment theme changes.
    // DirectMusic's default segment-switch flag is DMUS_SEGF_MEASURE: the new
    // segment is queued to start at the next measure boundary, not the next
    // pattern. A pattern in Gothic styles is frequently 4..16 measures, so
    // waiting for pattern-end can delay a zone change by 20-40 seconds (what
    // the user heard as "switch never happens"). Measure cadence is short
    // enough (1..2 s at typical tempi) to sound musical while still avoiding
    // the mid-phrase cut that produced the mush. Urgent embellishments
    // (FILL/BREAK/END/INTRO) still interrupt immediately.
    int64_t hardStop = patEnd;
    // Measure-boundary only applies to the INITIAL decision to transition
    // (NORMAL switch requested, no bridge in progress). Once a BREAK/END
    // bridge is playing we want it to run to its natural end — cutting it
    // at a measure boundary would re-enter nextPattern mid-bridge and
    // collapse the whole transition to the old behaviour.
    if(nextMus!=nullptr && embellishment.load()==DMUS_EMBELLISHT_NORMAL
       && !transitionBridgeActive) {
      const auto& ts = pat->styh.timeSig;
      const double bpm      = std::max(1.0, activeTempo);
      const double beatMs   = (60000.0/bpm) * (4.0/std::max<int>(1,int(ts.bBeat)));
      const double measureMs= beatMs * std::max<int>(1,int(ts.bBeatsPerMeasure));
      const int64_t spm     = int64_t(measureMs * double(SoundFont::SampleRate) / 1000.0);
      if(spm>0) {
        const int64_t off          = sampleCursor - patStart;
        const int64_t nextBoundary = patStart + ((off/spm) + 1) * spm;
        hardStop = std::min(patEnd, nextBoundary);
        }
      }

    const int64_t remain = std::min(hardStop-sampleCursor,int64_t(samplesRemain));
    const int64_t b      = (sampleCursor       );
    const int64_t e      = (sampleCursor+remain);

    auto& pptn         = *pat;
    const float mixVolume = cur->volume.load()*this->volume.load();

    const Step stp = stepInc(pptn,b,e,remain);
    implMix(pptn,mixVolume,out,size_t(stp.samples));

    if(remain!=stp.samples)
      stepApply(pat,stp,sampleCursor);

    sampleCursor += stp.samples;
    out          += stp.samples*2;
    samplesRemain-= size_t(stp.samples);

    // Skip silent pattern tail. Originally this guarded an addonworld.zen
    // specific quirk (odd padding at pattern end), but the guards are
    // generically correct and it's useful for any pattern: if there are
    // no note-ons left until patEnd *and* no instruments currently
    // sounding (uniqInstr empty), there's nothing audible between here
    // and patEnd — fast-forward to patEnd so the next pattern starts
    // immediately instead of rendering silence.
    if(stp.nextOn==std::numeric_limits<int64_t>::max() && uniqInstr.size()==0) {
      const int64_t nextUntilEnd = nextNoteOn(pptn, sampleCursor, patEnd);
      if(nextUntilEnd==std::numeric_limits<int64_t>::max())
        sampleCursor = patEnd;
      }

    // Switch conditions:
    //   • sampleCursor>=hardStop — natural pattern end OR pending measure
    //     boundary for a deferred NORMAL-embellishment theme switch.
    //   • urgentSwitch           — explicit FILL/BREAK/END/INTRO, interrupts
    //     mid-pattern like SEGF_DEFAULT in the DirectMusic reference.
    const bool urgentSwitch =
        (nextMus!=nullptr && embellishment.load()!=DMUS_EMBELLISHT_NORMAL);
    if(sampleCursor>=hardStop || urgentSwitch) {
      nextPattern();
      pat = pattern;
      if(pat==nullptr || !stp.isValid())
        break;
      }
    }

  uniqInstr.remove_if([](Instr& i){
    return i.counter==0 && !i.ptr->font.hasNotes();
    });

  // Update persistent channel cache while uniqInstr is still populated.
  // Only overwrite the cache when we have live data; stale data is kept between
  // patterns so 'music list' always shows the last known channels.
  if(!uniqInstr.empty()) {
    std::vector<ChannelState> snap;
    snap.reserve(uniqInstr.size());
    for(const auto& i : uniqInstr) {
      if(i.ptr==nullptr) continue;
      ChannelState state;
      state.pChannel      = i.ptr->key;
      state.dwPatch       = i.ptr->dwPatch;
      state.volume        = i.ptr->volume;
      state.pan           = i.ptr->pan;
      state.activeVoices  = i.counter;
      state.isMuted       = isChannelMuted(state.pChannel);
      state.isSolo        = (soloChannel>=0 && state.pChannel==uint32_t(soloChannel));
      state.dls           = i.ptr->dls;
      state.instrumentName= i.ptr->instrumentName;
      snap.emplace_back(std::move(state));
      }
    std::sort(snap.begin(), snap.end(), [](const ChannelState& a, const ChannelState& b){
      return a.pChannel < b.pChannel;
      });
    std::lock_guard<std::mutex> lk(cacheMutex_);
    channelCache_ = std::move(snap);
    }
  }

void Mixer::setVolume(float v) {
  volume.store(v);
  }

void Mixer::implMix(PatternInternal &pptn, float inVolume, int16_t *out, size_t cnt) {
  const size_t cnt2=cnt*2;
  pcm   .resize(cnt2);
  pcmMix.resize(cnt2);
  vol   .resize(cnt);
  pan   .resize(cnt);

  std::memset(pcmMix.data(),0,cnt2*sizeof(pcmMix[0]));

  for(auto& i:uniqInstr) {
    auto& ins = *i.ptr;
    if(!ins.font.hasNotes())
      continue;
    if(isChannelMuted(ins.key))
      continue;

    // Apply DMUS_CURVET_PBCURVE pitch bend (per-chunk coarse granularity).
    // pitchBendAt() latches the last value so held bends persist across
    // chunks. If the pattern has no PB curves but a previous pattern left
    // a non-zero latched bend, we still push a reset to zero on the first
    // chunk so stale bend doesn't leak between patterns.
    if(!pptn.pitchBend.empty()) {
      const float pb = pitchBendAt(pptn, i, sampleCursor);
      ins.font.setPitchBendNormalized(pb);
      }
    else if(i.pitchBendLast != 0.f) {
      i.pitchBendLast = 0.f;
      ins.font.setPitchBendNormalized(0.f);
      }

    std::memset(pcm.data(),0,cnt2*sizeof(pcm[0]));
    ins.font.mix(pcm.data(),cnt);

    float insVolume = ins.volume; // linear channel volume; v*v below gives the perceptual curve
    if(ins.key==5 || ins.key==6) {
      // HACK
      // insVolume*=0.10f;
      }
    const bool hasVol = hasVolumeCurves(pptn,i);
    const bool hasPan = hasPanCurves(pptn,i);

    if(hasVol)
      volFromCurve(pptn,i,vol);
    if(hasPan)
      panFromCurve(pptn,i,pan);

    if(!hasPan) {
      // Fast path — no time-varying pan, don't compound L/R gains.
      if(hasVol) {
        for(size_t r=0;r<cnt2;++r) {
          float v = vol[r/2];
          pcmMix[r] += pcm[r]*insVolume*(v*v);
          }
        } else {
        const float v = i.volLast;
        const float gain = insVolume*v*v;
        for(size_t r=0;r<cnt2;++r)
          pcmMix[r] += pcm[r]*gain;
        }
      } else {
      // Pan-curve path. Equal-power panning, normalised so that pan=0.5
      // yields (1,1) — i.e. "centre" is unity gain and does not re-attenuate
      // channels that the font already panned. This compounds with any
      // baked-in static voice pan; Gothic's band-level pan is almost always
      // centre so compounding is a non-issue for the typical case.
      constexpr float kHalfPi = kPi*0.5f;
      for(size_t r=0;r<cnt;++r) {
        const float v = hasVol ? vol[r] : i.volLast;
        const float p = pan[r];
        // cos/sin equal-power × √2 so that p=0.5 → gL=gR=1.
        const float gL = std::cos(p*kHalfPi) * 1.41421356f;
        const float gR = std::sin(p*kHalfPi) * 1.41421356f;
        const float g  = insVolume*v*v;
        pcmMix[r*2+0] += pcm[r*2+0]*g*gL;
        pcmMix[r*2+1] += pcm[r*2+1]*g*gR;
        }
      }
    }

  // I3DL2-style reverb post-processing.
  // Applied before master volume so that the reverb tail tracks the music
  // volume slider rather than being gated against it — matches how the
  // DirectX audio path mixes wet reverb pre-master.
  reverb_.process(pcmMix.data(), cnt);

  // Soft saturation: pass-through below ±knee, smooth asymptotic curve above.
  // At knee=0.85: x=1.0→0.91, x=1.5→0.95, x=2.0→0.97 — no harsh distortion.
  static constexpr float knee       = 0.85f;
  static constexpr float kneeFactor = 1.0f / (1.0f - knee); // ≈6.667
  for(size_t i=0;i<cnt2;++i) {
    float v = pcmMix[i]*inVolume;
    if(v >  knee) v =  knee + (v - knee) / (1.0f + (v - knee) * kneeFactor);
    else if(v < -knee) v = -(knee + (-v - knee) / (1.0f + (-v - knee) * kneeFactor));
    out[i] = int16_t(v * 32767.5f);
    }
  }

void Mixer::volFromCurve(PatternInternal &part,Instr& inst,std::vector<float> &v) {
  float& base = inst.volLast;
  for(auto& i:v)
    i=base;

  const int64_t curveShift = sampleCursor-patStart;

  for(auto& curve : part.volume) {
    if(curve.inst!=inst.ptr)
      continue;
    if(!checkVariation(curve))
      continue;

    int64_t s = toSamplesScaled(curve.at)-curveShift;
    int64_t e = toSamplesScaled(curve.at+curve.duration)-curveShift;
    if((s>=0 && size_t(s)>v.size()) || e<0)
      continue;

    const size_t begin = size_t(std::max<int64_t>(s,0));
    const size_t size  = std::min(size_t(e),v.size());
    const float  range = float(e-s);
    const float  diffV = curve.endV-curve.startV;
    const float  startV = curve.startV;
    const float  endV  = curve.endV;

    switch(curve.shape) {
      case DMUS_CURVES_LINEAR: {
        for(size_t sampleIdx=begin;sampleIdx<size;++sampleIdx) {
          float val = (float(sampleIdx)-float(s))/range;
          v[sampleIdx] = val*diffV+startV;
          }
        break;
        }
      case DMUS_CURVES_INSTANT: {
        for(size_t sampleIdx=begin;sampleIdx<size;++sampleIdx) {
          v[sampleIdx] = endV;
          }
        break;
        }
      case DMUS_CURVES_EXP: {
        for(size_t sampleIdx=begin;sampleIdx<size;++sampleIdx) {
          float val = (float(sampleIdx)-float(s))/range;
          v[sampleIdx] = std::pow(val,2.f)*diffV+startV;
          }
        break;
        }
      case DMUS_CURVES_LOG: {
        for(size_t sampleIdx=begin;sampleIdx<size;++sampleIdx) {
          float val = (float(sampleIdx)-float(s))/range;
          v[sampleIdx] = std::sqrt(val)*diffV+startV;
          }
        break;
        }
      case DMUS_CURVES_SINE: {
        for(size_t sampleIdx=begin;sampleIdx<size;++sampleIdx) {
          float linear = (float(sampleIdx)-float(s))/range;
          float val    = std::sin(kPi*linear*0.5f);
          v[sampleIdx] = val*diffV+startV;
          }
        break;
        }
      }
    if(size>begin)
      base = v[size-1];
    }
  }

int Mixer::getGroove() const {
  auto  pmus = current;
  if(pmus==nullptr)
    return 0;
  auto& mus  = *pmus;
  if(mus.groove.size()==0)
    return 0;
  auto& g = mus.groove[grooveCounter.load()%mus.groove.size()];
  return g.bGrooveLevel;
  }

bool Mixer::hasVolumeCurves(Mixer::PatternInternal& part, Mixer::Instr& inst) const {
  for(auto& i:part.volume) {
    if(i.inst!=inst.ptr)
      continue;
    if(!checkVariation(i))
      continue;
    return true;
    }
  return false;
  }

bool Mixer::hasPanCurves(Mixer::PatternInternal& part, Mixer::Instr& inst) const {
  for(auto& i:part.pan) {
    if(i.inst!=inst.ptr)
      continue;
    if(!checkVariation(i))
      continue;
    return true;
    }
  return false;
  }

// Structural twin of volFromCurve, but evaluating CC10 Pan curves.
// Output is in [0..1]: 0=fully left, 0.5=centre, 1=fully right — matches
// DxSynth::Synth::setPan convention.
void Mixer::panFromCurve(PatternInternal& part, Instr& inst, std::vector<float>& v) {
  float& base = inst.panLast;
  for(auto& i:v)
    i = base;

  const int64_t curveShift = sampleCursor-patStart;

  for(auto& curve : part.pan) {
    if(curve.inst!=inst.ptr)
      continue;
    if(!checkVariation(curve))
      continue;

    int64_t s = toSamplesScaled(curve.at)-curveShift;
    int64_t e = toSamplesScaled(curve.at+curve.duration)-curveShift;
    if((s>=0 && size_t(s)>v.size()) || e<0)
      continue;

    const size_t begin  = size_t(std::max<int64_t>(s,0));
    const size_t size   = std::min(size_t(e),v.size());
    const float  range  = float(e-s);
    const float  diffV  = curve.endV-curve.startV;
    const float  startV = curve.startV;
    const float  endV   = curve.endV;

    switch(curve.shape) {
      case DMUS_CURVES_LINEAR: {
        for(size_t r=begin;r<size;++r) {
          float t = (float(r)-float(s))/range;
          v[r] = t*diffV+startV;
          }
        break;
        }
      case DMUS_CURVES_INSTANT: {
        for(size_t r=begin;r<size;++r)
          v[r] = endV;
        break;
        }
      case DMUS_CURVES_EXP: {
        for(size_t r=begin;r<size;++r) {
          float t = (float(r)-float(s))/range;
          v[r] = std::pow(t,2.f)*diffV+startV;
          }
        break;
        }
      case DMUS_CURVES_LOG: {
        for(size_t r=begin;r<size;++r) {
          float t = (float(r)-float(s))/range;
          v[r] = std::sqrt(t)*diffV+startV;
          }
        break;
        }
      case DMUS_CURVES_SINE: {
        for(size_t r=begin;r<size;++r) {
          float t   = (float(r)-float(s))/range;
          float val = std::sin(kPi*t*0.5f);
          v[r] = val*diffV+startV;
          }
        break;
        }
      }
    if(size>begin)
      base = v[size-1];
    }
  }

bool Mixer::hasPitchBendCurves(Mixer::PatternInternal& part, Mixer::Instr& inst) const {
  for(auto& i:part.pitchBend) {
    if(i.inst!=inst.ptr)
      continue;
    if(!checkVariation(i))
      continue;
    return true;
    }
  return false;
  }

// Evaluate the active pitch-bend curve value at `sampleAbs` (absolute sample
// cursor, same units as sampleCursor/patStart). Returns normalized bend
// [-1..+1]; centre is 0. Used for per-chunk coarse updates — chunks are short
// enough (≤ one MIDI event between them) that per-sample interpolation inside
// a chunk is overkill. The last evaluated value is latched into inst.pitchBendLast
// so that when no curve is currently active the held value persists across chunks.
float Mixer::pitchBendAt(PatternInternal& part, Instr& inst, int64_t sampleAbs) {
  float v = inst.pitchBendLast;
  for(auto& curve : part.pitchBend) {
    if(curve.inst!=inst.ptr)
      continue;
    if(!checkVariation(curve))
      continue;

    const int64_t s = toSamplesScaled(curve.at) + patStart;
    const int64_t e = toSamplesScaled(curve.at + curve.duration) + patStart;

    if(sampleAbs < s)
      continue;         // curve hasn't started yet for this chunk
    if(sampleAbs >= e) {
      v = curve.endV;   // curve finished; latch its end value
      continue;
      }

    const float  range  = float(e - s);
    const float  t      = range > 0.f ? float(sampleAbs - s) / range : 0.f;
    const float  diffV  = curve.endV - curve.startV;
    const float  startV = curve.startV;
    const float  endV   = curve.endV;

    switch(curve.shape) {
      case DMUS_CURVES_LINEAR:  v = t*diffV + startV; break;
      case DMUS_CURVES_INSTANT: v = endV; break;
      case DMUS_CURVES_EXP:     v = std::pow(t,2.f)*diffV + startV; break;
      case DMUS_CURVES_LOG:     v = std::sqrt(t)*diffV + startV; break;
      case DMUS_CURVES_SINE:    v = std::sin(kPi*t*0.5f)*diffV + startV; break;
      }
    }
  inst.pitchBendLast = v;
  return v;
  }

template<class T>
bool Mixer::checkVariation(const T& item) const {
  if(item.inst==nullptr)
    return false;
  if(item.inst->dwVarCount==0 || item.dwVariation==0u)
    return true;

  const uint32_t vbit = item.inst->activeVariation;
  if(vbit>=32u)
    return true;
  if((item.dwVariation & (1u<<vbit))==0u)
    return false;
  return true;
  }
