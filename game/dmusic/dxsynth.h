#pragma once
// dxsynth.h — Authentic NT5 dmsynth port (cross-platform C++17)
//
// Faithful reimplementation of Microsoft DirectMusic dmsynth DSP algorithms.
// Reference: tongzx/nt5src XPSP1/NT/multimedia/directx/dmusic/dmsynth/
//   voice.cpp  – CVoiceEG, CVoiceLFO, CVoiceFilter, CDigitalAudio
//   mix.cpp    – Mix16() 20.12 fixed-point interpolation
//   mixf.cpp   – Mix16Filtered() all-pole IIR filter
//
// NT5 type aliases used here:
//   PFRACT = int32_t  (20.12 fixed-point, 4096 = 1.0 sample step)
//   VFRACT = int32_t  (volume, 4095 = 0 dB; used with >>13 in mix loop)
//   COEFF  = int32_t  (Q2.30 filter coefficient, 1<<30 = 1.0)
//   STIME  = int64_t  (absolute sample counter)
//   PREL   = int32_t  (pitch cents, relative)
//   VREL   = int32_t  (volume 1/100 dB; 0 = 0 dB, −9600 = −96 dB)

#include "dlscollection.h"   // provides DlsCollection::Instrument, Wave (via wave.h)

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace Dx8 {
namespace DxSynth {

// ── NT5 dmsynth fixed-point types ─────────────────────────────────────────────
using PFRACT = int32_t;
using VFRACT = int32_t;
using COEFF  = int32_t;
using STIME  = int64_t;
using PREL   = int32_t;
using VREL   = int32_t;

// ── Articulation parameters (parsed from DLS ConnectionBlocks) ─────────────────
struct ArticParams {
  // EG1 — amplitude envelope (times in seconds, sustain linear 0..1)
  float eg1Delay      = 0.f;
  float eg1Attack     = 0.001f;
  float eg1Hold       = 0.f;
  float eg1Decay      = 1.f;
  float eg1Sustain    = 1.f;
  float eg1Release    = 0.3f;
  float eg1VelAttack  = 0.f;    // timecents/velocity
  float eg1KeyToHold  = 0.f;    // timecents/key (from C4)
  float eg1KeyToDecay = 0.f;
  // EG2 — modulation envelope
  float eg2Delay      = 0.f;
  float eg2Attack     = 0.001f;
  float eg2Hold       = 0.f;
  float eg2Decay      = 1.f;
  float eg2Sustain    = 1.f;
  float eg2Release    = 0.3f;
  float eg2VelAttack  = 0.f;
  float eg2KeyToHold  = 0.f;
  float eg2KeyToDecay = 0.f;
  // EG2 routing
  float eg2ToPitch    = 0.f;    // PREL cents at EG2 level 1000
  float eg2ToFilter   = 0.f;    // PREL cents at EG2 level 1000
  // LFO (pitch/volume/filter modulation)
  float lfoFreqHz     = 5.f;
  float lfoDelaySec   = 0.f;
  float lfoToPitch    = 0.f;    // PREL cents at LFO level ±100
  float lfoToVolume   = 0.f;    // VREL per LFO unit (1/100 dB per ±1)
  float lfoToFilter   = 0.f;    // PREL cents at LFO level ±100
  // Vibrato LFO (pitch only)
  float vibFreqHz     = 5.f;
  float vibDelaySec   = 0.f;
  float vibToPitch    = 0.f;
  // Filter
  float filterCutHz   = 20000.f; // Hz
  float filterQ       = 0.707f;
  // Volume / pan / tuning
  float attenuationDB = 0.f;     // positive = quieter
  float velToAtten    = 0.f;     // centibels of extra attenuation at vel=0 (DLS kSrcVelocity→kDstAtten)
  float panValue      = 0.f;     // −1.0..+1.0 (−1=full left, 0=centre, +1=full right)
  float scaleTuning   = 100.f;   // cents/semitone
  };

// ── WaveBank — all DLS waves decoded to int16 mono, shared ────────────────────
// int16 matches NT5 m_pnWave (short*) and avoids float conversion overhead.
// WaveMeta captures per-wave sample parameters from Wave::WaveSample and
// Wave::loop BEFORE the wave vector is cleared by DlsCollection (keepWaveData=false).
// This allows compileInstr to run safely even after the originating DlsCollection
// has been moved and its wave vector cleared.
struct WaveBank {
  // ── Per-wave sample metadata (captured at build time) ─────────────────────
  struct WaveMeta {
    uint32_t cbSize       = 0;    // waveSample.cbSize; 0 = not explicitly set
    uint16_t unityNote    = 60;   // waveSample.usUnityNote
    int16_t  fineTune     = 0;    // waveSample.sFineTune (cents)
    int32_t  lAttenuation = 0;    // waveSample.lAttenuation (centibels, 1 cB = 0.1 dB; positive = quieter)
    bool     hasLoop      = false;
    uint32_t loopType     = 0;    // 0=loop, 1=sustain-loop (matches DLS spec)
    uint32_t loopStart    = 0;    // sample index
    uint32_t loopLength   = 0;    // sample count
    };

  std::vector<int16_t>  pcm;          // all waves packed: int16 mono
  std::vector<uint32_t> offsets;      // start index in pcm[] per wave
  std::vector<uint32_t> lengths;      // sample count per wave
  std::vector<uint32_t> sampleRates;  // original sample rate per wave
  std::vector<WaveMeta> meta;         // per-wave metadata (same indexing)

  static std::shared_ptr<WaveBank> build(const std::vector<Wave>& waves);
  };

// ── CompiledRegion — one DLS region ready to play ─────────────────────────────
struct CompiledRegion {
  uint8_t  keyLo = 0,   keyHi = 127;
  uint8_t  velLo = 0,   velHi = 127;
  uint16_t keyGroup      = 0;
  uint8_t  unityNote     = 60;
  int16_t  fineTuneCents = 0;
  float    attenuationDB = 0.f;
  uint32_t sampleRate    = 44100;
  // Direct pointer into WaveBank::pcm (int16, NT5-style)
  const int16_t* samples    = nullptr;
  uint32_t       numSamples = 0;
  // Loop
  bool     looping     = false;
  bool     loopSustain = false;
  uint32_t loopStart   = 0;
  uint32_t loopEnd     = 0;
  ArticParams art;
  };

// ── CompiledInstr — all regions for one DLS patch ─────────────────────────────
struct CompiledInstr {
  std::vector<CompiledRegion> regions;
  const CompiledRegion* findRegion(uint8_t key, uint8_t vel) const;
  };

// ── EnvGen — NT5 CVoiceEG: STIME-based ADSR with logarithmic attack ───────────
// Level = 0..1000 permille. noteOff() scales release to current sustain level
// (identical to CVoiceEG::StopVoice).
struct EnvGen {
  STIME   startTime  = 0;
  STIME   stopTime   = std::numeric_limits<STIME>::max();
  STIME   stDelay    = 0;
  STIME   stAttack   = 44;
  STIME   stHold     = 0;
  STIME   stDecay    = 44100;
  int32_t pcSustain  = 1000;  // permille 0..1000
  STIME   stRelease  = 13230;
  // Level at the instant noteOff() was called. Used as the starting amplitude
  // for the release phase instead of pcSustain — NT5's original CVoiceEG
  // evaluates GetLevel(m_stStopTime) on the fly, we cache it so a noteOff
  // arriving during Attack (level < pcSustain) doesn't produce a click when
  // the release jumps the level back up to pcSustain before fading.
  int32_t pcReleaseStart = 1000;
  bool    enabled    = false;
  bool    isVolEG    = false; // true → use log attack table in attack/hold

  void    setup(const ArticParams& art, uint8_t key, uint8_t vel,
                bool isEG1, int sampleRate);
  void    noteOn (STIME t);
  void    noteOff(STIME t); // scales stRelease by current level

  // Returns 0..1000.  *pstNext = samples until next significant change.
  int32_t getLevel(STIME t, STIME* pstNext) const;

  // Returns VREL (0=0dB .. −9600=−96dB). Calls getLevel(fVolume=true).
  VREL    getVolume(STIME t, STIME* pstNext) const;

  bool    isActive(STIME t) const;
  };

// ── LFOGen — NT5 CVoiceLFO: 256-entry sine table ─────────────────────────────
// getLevel() returns −100..+100. Pitch/volume derived by caller.
struct LFOGen {
  STIME  startTime  = 0;
  STIME  delayTime  = 0;   // samples before oscillation starts
  STIME  repeatTime = 44100; // period/256 — used as next-step hint
  PFRACT frequency  = 0;   // Q16.16: cycles/sample × (256 << 16)

  void   init(float freqHz, float delaySec, int sampleRate, STIME startTime);
  // Returns −100..+100 (same scale as NT5 m_snSineTable).
  int32_t getLevel(STIME t, STIME* pstNext) const;
  };

// ── AllPoleFilter — NT5 CVoiceFilter: y = K·x + B1·y1 − B2·y2 ────────────────
// B1 stored as −b1 (positive for lowpass, as in NT5).
// State vars z1/z2 = previous output samples (Direct Form I).
// Integer arithmetic: MulDiv(a,b,1<<30) = (int64)a*b >> 30.
struct AllPoleFilter {
  bool   active = false;
  COEFF  cfK  = 0x40000000;  // Q2.30, 1<<30 = 1.0 (pass-through)
  COEFF  cfB1 = 0;            // stored positive (negated b1)
  COEFF  cfB2 = 0;
  int32_t z1 = 0, z2 = 0;

  // Compute coefficients. Same pole-placement as NT5 reference GetCoeffRef().
  void    setup(float cutoffHz, float Q, int sampleRate);
  void    disable() { active = false; cfK = 0x40000000; cfB1 = cfB2 = z1 = z2 = 0; }
  // Apply one sample. Returns filtered value at same int16 scale.
  int32_t tick(int32_t x);
  };

// ── Voice — one active note ───────────────────────────────────────────────────
struct Voice {
  bool     active    = false;
  bool     releasing = false;
  uint8_t  note      = 60;
  uint8_t  vel       = 64;
  uint16_t keyGroup  = 0;
  STIME    sampleTime = 0;  // absolute time counter, starts at noteOn globalTime_
  STIME    birthTime  = 0;  // globalTime_ at noteOn — priority for voice stealing

  // Sample data — int16 pointer into WaveBank::pcm (NT5: m_pnWave)
  const int16_t* samples    = nullptr;
  uint32_t       numSamples = 0;
  PFRACT   samplePos  = 0;  // 20.12 fixed-point position (4096 = 1 sample step)
  PFRACT   basePitch  = 0;  // base pitch increment per output sample
  bool     looping     = false;
  bool     loopSustain = false;
  uint32_t loopStart   = 0;
  uint32_t loopEnd     = 0;

  // Static gain pre-computed at noteOn (attenuation + velocity + pan).
  // VFRACT units: 4095 = 0dB. Actual per-sample gain = staticL/R * EG1/1000.
  VFRACT   staticL = 0;
  VFRACT   staticR = 0;

  // Modulation routing (in PREL / VREL, matching NT5 source units)
  PREL  eg2ToPitch  = 0;
  PREL  eg2ToFilter = 0;
  PREL  lfoToPitch  = 0;
  VREL  lfoToVolume = 0;
  PREL  lfoToFilter = 0;
  PREL  vibToPitch  = 0;

  // Filter base parameters
  float    filterCutBase = 20000.f;
  float    filterQ       = 0.707f;
  int      outSampleRate = 44100;

  // DSP components
  EnvGen        eg1, eg2;
  LFOGen        lfo, vib;
  AllPoleFilter filter;

  // Render one sample into outL/outR (accumulate). Returns false when done.
  // extraPitchCents: additional pitch modulation in cents (e.g. for pitch bend).
  bool render(float& outL, float& outR, PREL extraPitchCents = 0);
  void release();
  void kill();
  };

// ── Synth — polyphonic voice pool ────────────────────────────────────────────
class Synth {
  public:
    static constexpr int kMaxVoices = 64;

    Synth(std::shared_ptr<WaveBank> bank, CompiledInstr instr, int outSampleRate);

    void noteOn (uint8_t note, uint8_t vel);
    void noteOff(uint8_t note);
    void setPan (float pan);   // 0..1 (0.5 = centre)
    // Pitch bend inputs.
    //   setPitchBend(raw14): raw 14-bit MIDI pitch bend (0..16383, centre=8192).
    //   setPitchBendNormalized(n): already-centred bend in [-1..+1]. Used for
    //     DMUS_CURVET_PBCURVE curves that patternlist.cpp re-centres at parse time.
    // Range fixed at ±200 cents (±2 semitones) — the General MIDI default.
    void setPitchBend(uint16_t raw14);
    void setPitchBendNormalized(float n);
    bool hasNotes() const;
    // Accumulate into stereo interleaved float buf[L0,R0,L1,R1,…]
    void render(float* buf, int frames);

  private:
    Voice* allocVoice();
    void   killGroup(uint16_t group);

    std::shared_ptr<WaveBank>     bank_;
    CompiledInstr                 instr_;
    int                           outRate_    = 44100;
    float                         pan_        = 0.f;   // −0.5..+0.5
    float                         pitchBendCents_      = 0.f;   // current bend in cents
    float                         pitchBendRangeCents_ = 200.f; // ±200 cents (GM default)
    STIME                         globalTime_ = 0;
    Voice                         voices_[kMaxVoices];
  };

// ── compileInstr ──────────────────────────────────────────────────────────────
// Compiles one DLS instrument into a Synth-ready CompiledInstr.
// Takes instruments by value (or as a ref to a stored copy) so it is safe to
// call even after the originating DlsCollection has been moved or destroyed.
// Wave metadata comes from bank->meta (populated during WaveBank::build while
// the Wave vector was still available).
CompiledInstr compileInstr(const std::vector<DlsCollection::Instrument>& instruments,
                            uint32_t                                       dwPatch,
                            const std::shared_ptr<WaveBank>&               bank);

} // namespace DxSynth
} // namespace Dx8
