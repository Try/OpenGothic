// dxsynth.cpp — Authentic NT5 dmsynth algorithms (cross-platform C++17)
//
// All DSP algorithms taken verbatim from NT5 dmsynth source
// (tongzx/nt5src XPSP1/NT/multimedia/directx/dmusic/dmsynth/).
// Windows API replaced with portable equivalents:
//   - LONGLONG  → int64_t
//   - MulDiv    → (int64_t)a * b >> 30
//   - _asm{}    → not defined (_X86_ not set → portable C++ paths used)
//   - COM/CRITICAL_SECTION → removed; output via Tempest float buffer.

#include "dxsynth.h"
#include "dlscollection.h"
#include "wave.h"

#include <Tempest/Log>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

using namespace Dx8;
using namespace Dx8::DxSynth;
using namespace Tempest;

static constexpr float kPi = 3.14159265358979323846f;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers: timecents / pitch cents conversions  (identical to NT5 CDigitalAudio)
// ─────────────────────────────────────────────────────────────────────────────

// timecents → seconds  (DLS: 0 tc = 1 s, −12000 tc ≈ 1 ms)
static float tc2sec(float tc) {
  tc = std::max(-12000.f, std::min(8000.f, tc));
  return std::pow(2.f, tc / 1200.f);
  }

// pitch cents → ratio  (100 cents = 1 semitone)
static float cents2ratio(float c) {
  return std::pow(2.f, c / 1200.f);
  }

// Convert VREL (1/100 dB) to VFRACT (4095 = 0 dB).
// NT5: VRELToVFRACT via m_svfDbToVolume table (sqrt of linear amplitude * 4095).
// Equivalent formula: 4095 * sqrt(pow(10, vrel/1000))
// where vrel units: 100 units = 1 dB, 0 = 0 dB, −9600 = −96 dB.
static VFRACT vrelToVfract(VREL vrel) {
  if(vrel <= -9600)
    return 0;
  double dB = double(vrel) / 100.0; // 100 VREL = 1 dB
  double lin = std::pow(10.0, dB / 20.0); // amplitude linear
  return VFRACT(lin * 4095.0);
  }

// NT5 PRELToPFRACT: pitch cents → PFRACT (4096 = 1.0 pitch ratio).
// Uses decomposition into octaves + semitones + cents for precision.
static PFRACT prelToPfract(PREL prPitch) {
  // Clamp to ±4800 cents (±4 octaves)
  if(prPitch > 4800)  prPitch = 4800;
  if(prPitch < -4800) prPitch = -4800;
  double ratio = std::pow(2.0, prPitch / 1200.0);
  return PFRACT(ratio * 4096.0);
  }

// ─────────────────────────────────────────────────────────────────────────────
// NT5 EG1 volume lookup table: level (0..1000) → VFRACT (0..4095)
// Maps: VREL = level*96/10 − 9600, then VFRACT = sqrt(10^(VREL/1000)) * 4095
// Precomputed at first use; 1001 entries (one per permille).
// Avoids pow() in the per-sample hot path while matching NT5 GetVolume/VRELToVFRACT.
// ─────────────────────────────────────────────────────────────────────────────
static const VFRACT* eg1GainTable() {
  static VFRACT tbl[1001];
  static bool   ready = false;
  if(!ready) {
    for(int lvl = 0; lvl <= 1000; ++lvl) {
      VREL   vr  = VREL(lvl) * 96 / 10 - 9600;   // same as EnvGen::getVolume
      tbl[lvl]   = vrelToVfract(vr);
      }
    ready = true;
    }
  return tbl;
  }

// ─────────────────────────────────────────────────────────────────────────────
// NT5 CVoiceLFO::Init() — 256-entry sine table, values ±100
// Verbatim formula: sin(i * 2π/256) * 100  stored as short
// ─────────────────────────────────────────────────────────────────────────────
static const int16_t* sineTable() {
  static int16_t tbl[256];
  static bool    ready = false;
  if(!ready) {
    for(int i = 0; i < 256; ++i) {
      double f = double(i) * 6.283185307 / 256.0;
      tbl[i] = int16_t(std::sin(f) * 100.0);
      }
    ready = true;
    }
  return tbl;
  }

// ─────────────────────────────────────────────────────────────────────────────
// NT5 CVoiceEG::Init() — 201-entry logarithmic attack table
// Verbatim formula:
//   tbl[0] = 0
//   tbl[lV] = (short)(log10((lV/200)^2) * 10000/96 + 1000)  for lV = 1..200
// Maps attack progress index 0..200 (= permille/5) to dB-scaled level 0..1000.
// ─────────────────────────────────────────────────────────────────────────────
static const int16_t* attackTable() {
  static int16_t tbl[201];
  static bool    ready = false;
  if(!ready) {
    tbl[0] = 0;
    for(int lV = 1; lV <= 200; ++lV) {
      double f = double(lV) / 200.0;
      f = f * f;                   // square
      f = std::log10(f);           // log10
      f = f * 10000.0 / 96.0;     // scale to −10000/96 .. 0
      f = f + 1000.0;              // shift so max = 1000
      tbl[lV] = int16_t(f);
      }
    ready = true;
    }
  return tbl;
  }

// ─────────────────────────────────────────────────────────────────────────────
// DLS / SF2 connection-block source & destination codes
// ─────────────────────────────────────────────────────────────────────────────
enum : uint16_t {
  kSrcNone     = 0x0000,
  kSrcLFO      = 0x0001,
  kSrcVelocity = 0x0002,
  kSrcKeyNum   = 0x0003,
  kSrcEG2      = 0x0005,
  kSrcVibrato  = 0x0009,

  kDstAtten    = 0x0001,
  kDstPitch    = 0x0003,
  kDstPan      = 0x0004,
  kDstLFOFreq  = 0x0104,
  kDstLFODelay = 0x0105,
  kDstVibFreq  = 0x0114,
  kDstVibDelay = 0x0115,
  kDstEG1Att   = 0x0206,
  kDstEG1Dec   = 0x0207,
  kDstEG1Rel   = 0x0209,
  kDstEG1Sus   = 0x020a,
  kDstEG1Dly   = 0x020b,
  kDstEG1Hold  = 0x020c,
  kDstEG2Att   = 0x030a,
  kDstEG2Dec   = 0x030b,
  kDstEG2Rel   = 0x030d,
  kDstEG2Sus   = 0x030e,
  kDstEG2Dly   = 0x030f,
  kDstEG2Hold  = 0x0310,
  kDstFiltCut  = 0x0500,
  kDstFiltQ    = 0x0501,
  };

// ─────────────────────────────────────────────────────────────────────────────
// EnvGen — NT5 CVoiceEG
// ─────────────────────────────────────────────────────────────────────────────

void EnvGen::setup(const ArticParams& art, uint8_t key, uint8_t vel,
                   bool isEG1, int sampleRate) {
  isVolEG = isEG1;
  const float sr = float(sampleRate);

  float delayS   = isEG1 ? art.eg1Delay      : art.eg2Delay;
  float attackS  = isEG1 ? art.eg1Attack     : art.eg2Attack;
  float holdS    = isEG1 ? art.eg1Hold       : art.eg2Hold;
  float decayS   = isEG1 ? art.eg1Decay      : art.eg2Decay;
  float sustainV = isEG1 ? art.eg1Sustain    : art.eg2Sustain;
  float releaseS = isEG1 ? art.eg1Release    : art.eg2Release;
  float velAtt   = isEG1 ? art.eg1VelAttack  : art.eg2VelAttack;
  float keyHold  = isEG1 ? art.eg1KeyToHold  : art.eg2KeyToHold;
  float keyDecay = isEG1 ? art.eg1KeyToDecay : art.eg2KeyToDecay;

  // Key-number scaling: hold and decay scale by 2^(keyDelta*tc/1200)
  const float keyDelta = float(int(key) - 60);
  if(keyHold  != 0.f) holdS  *= cents2ratio(keyDelta * keyHold);
  if(keyDecay != 0.f) decayS *= cents2ratio(keyDelta * keyDecay);

  // Velocity → attack scaling (NT5: m_stAttack *= PRELToPFRACT(vel*scale/127) / 4096)
  if(velAtt != 0.f) {
    float velFrac = float(int(vel)) * velAtt / 127.f;
    attackS = tc2sec(std::log2(std::max(attackS, 1e-6f)) * 1200.f + velFrac);
    }

  stDelay   = STIME(std::max(0.f,    delayS)   * sr);
  stAttack  = STIME(std::max(0.001f, attackS)  * sr);
  stHold    = STIME(std::max(0.f,    holdS)    * sr);
  stDecay   = STIME(std::max(0.001f, decayS)   * sr);
  pcSustain = int32_t(std::max(0.f, std::min(1.f, sustainV)) * 1000.f);
  stRelease = STIME(std::max(0.001f, releaseS) * sr);

  // NT5: m_stDecay *= (1000 - m_pcSustain) / 1000  (decay is shorter for high sustain)
  stDecay = stDecay * STIME(1000 - pcSustain) / 1000;
  if(stDecay < 1) stDecay = 1;

  enabled = false;
  }

void EnvGen::noteOn(STIME t) {
  startTime = t;
  stopTime  = std::numeric_limits<STIME>::max();
  enabled   = true;
  }

// NT5 CVoiceEG::StopVoice: scale release by current level, then set stopTime.
void EnvGen::noteOff(STIME t) {
  if(!enabled) return;
  STIME dummy;
  int32_t level = getLevel(t, &dummy);
  // Cache the live level as the release-phase starting amplitude. Without
  // this, the release phase below always fades from pcSustain down to 0,
  // which is LOUDER than `level` if noteOff fires during Attack — producing
  // a click. Clamp to a small non-zero minimum so voices don't get stuck
  // audibly for 0-level edge cases, and so VREL conversion (level*96/10)
  // stays well-defined.
  pcReleaseStart = std::max(1, level);
  // Scale release proportionally to current level so fade-out time matches
  // how much amplitude there is to actually fade.
  stRelease = stRelease * STIME(level) / 1000;
  if(stRelease < 1) stRelease = 1;
  stopTime = t;
  }

// NT5 CVoiceEG::GetLevel — verbatim algorithm, STIME-based
int32_t EnvGen::getLevel(STIME stCurrent, STIME* pstNext) const {
  if(!enabled) { *pstNext = 44100; return 0; }
  const int16_t* atbl = attackTable();

  if(stCurrent <= stopTime) {
    STIME stEnd = stCurrent - startTime;

    // Delay phase
    if(stEnd < stDelay) {
      *pstNext = stDelay - stEnd;
      return 0;
      }
    stEnd -= stDelay;

    // Attack phase
    if(stEnd < stAttack) {
      int32_t lLevel = int32_t(1000 * stEnd / stAttack);
      if(lLevel < 0)    lLevel = 0;
      if(lLevel > 1000) lLevel = 1000;
      *pstNext = stAttack - stEnd;
      if(isVolEG) lLevel = atbl[lLevel / 5];
      return lLevel;
      }
    stEnd -= stAttack;

    // Hold phase
    if(stEnd < stHold) {
      *pstNext = stHold - stEnd;
      return isVolEG ? int32_t(atbl[200]) : 1000;
      }
    stEnd -= stHold;

    // Decay phase (LINEAR, same as NT5)
    if(stEnd < stDecay) {
      // Decay linearly from 1000 to pcSustain
      int32_t lLevel = 1000 - int32_t((1000 - pcSustain) * stEnd / stDecay);
      // Provide next-step hints at 1/4 and 1/2 of decay (NT5 heuristic)
      if(stEnd < (stDecay / 4 - 100))      *pstNext = stDecay / 4 - stEnd;
      else if(stEnd < (stDecay / 2 - 100)) *pstNext = stDecay / 2 - stEnd;
      else                                  *pstNext = stDecay - stEnd;
      return std::max(int32_t(pcSustain), lLevel);
      }

    // Sustain phase
    *pstNext = 44100;
    return pcSustain;
    }
  else {
    // Release phase
    STIME stEnd = stCurrent - stopTime;
    if(stEnd >= stRelease) {
      *pstNext = std::numeric_limits<STIME>::max() / 2;
      return 0;
      }
    // Release starts from the level captured at noteOff time, not pcSustain.
    // That way a noteOff during Attack (level < pcSustain) doesn't snap up
    // and then fade down — it fades smoothly from wherever the envelope
    // actually was when the key was released. Real NT5 does this by
    // recursively calling GetLevel(m_stStopTime); pcReleaseStart is the
    // cached equivalent (see noteOff()).
    int32_t startLevel = pcReleaseStart;
    int32_t lLevel = startLevel * int32_t(stRelease - stEnd) / int32_t(stRelease);
    if(stEnd < (stRelease / 4 - 100))      *pstNext = stRelease / 4 - stEnd;
    else if(stEnd < (stRelease / 2 - 100)) *pstNext = stRelease / 2 - stEnd;
    else                                    *pstNext = stRelease - stEnd;
    return std::max(0, lLevel);
    }
  }

// NT5 CVoiceEG::GetVolume: level 0..1000 → VREL 0..−9600
VREL EnvGen::getVolume(STIME t, STIME* pstNext) const {
  int32_t level = getLevel(t, pstNext);
  // NT5: vrLevel = GetLevel()*96/10 - 9600
  VREL vr = VREL(level) * 96 / 10 - 9600;
  return vr;
  }

bool EnvGen::isActive(STIME t) const {
  if(!enabled) return false;
  if(t <= stopTime)   return true;
  return (t - stopTime) < stRelease;
  }

// ─────────────────────────────────────────────────────────────────────────────
// LFOGen — NT5 CVoiceLFO
// ─────────────────────────────────────────────────────────────────────────────

void LFOGen::init(float freqHz, float delaySec, int sampleRate, STIME startT) {
  startTime = startT;
  delayTime = STIME(delaySec * float(sampleRate));

  if(freqHz < 0.001f) freqHz = 0.001f;
  // NT5: m_pfFrequency = cycles/sample * 256 * 65536  (Q16.16 × 256 table entries)
  // GetLevel: stTime * pfFreq >> (12+4) = stTime * pfFreq >> 16 → table index
  // So pfFreq = freqHz/sampleRate * 256 * 65536 = freqHz/sampleRate * (1<<24)/256 * 256
  frequency = PFRACT(freqHz / float(sampleRate) * 256.0f * 65536.0f);
  if(frequency < 1) frequency = 1;

  // NT5: m_stRepeatTime = 2097152 / m_pfFrequency  (period / 8 as next-step hint)
  repeatTime = STIME(2097152) / STIME(std::max(PFRACT(1), frequency));
  if(repeatTime < 1) repeatTime = 1;
  }

// NT5 CVoiceLFO::GetLevel — verbatim
int32_t LFOGen::getLevel(STIME stCurrent, STIME* pstNext) const {
  STIME stTime = stCurrent - (startTime + delayTime);
  if(stTime < 0) {
    *pstNext = -stTime;
    return 0;
    }
  *pstNext = repeatTime;
  // NT5: stTime *= m_pfFrequency; stTime >>= (12+4);
  stTime = (stTime * STIME(frequency)) >> 16;
  return int32_t(sineTable()[stTime & 0xFF]);
  }

// ─────────────────────────────────────────────────────────────────────────────
// AllPoleFilter — NT5 CVoiceFilter
// Formula: z = K*s + B1*z1 − B2*z2   (B1 stored as +|b1|)
// Reference: voice.cpp GetCoeffRef() comment block
//   b1 = −2*r*cos(theta),  b2 = r*r,  K = (1+b1+b2)*pow(10, −qIdx*0.0375)
// where theta = 2π*fc/fs,  r = exp(−theta/(2*Q))
// ─────────────────────────────────────────────────────────────────────────────

void AllPoleFilter::setup(float cutoffHz, float Q, int sampleRate) {
  z1 = z2 = 0;
  const float nyq = float(sampleRate) * 0.499f;
  if(cutoffHz <= 20.f || cutoffHz >= nyq || Q <= 0.f) {
    disable();
    active = (cutoffHz >= 20.f && cutoffHz < nyq);
    return;
    }
  active = true;

  const double theta = 2.0 * kPi * cutoffHz / double(sampleRate);
  const double r     = std::exp(-theta / (2.0 * double(Q)));
  const double b1    = -2.0 * r * std::cos(theta);  // b1 (negative for LP)
  const double b2    =  r * r;
  // K for unity DC gain (NT5 formula at Q=0 resonance: K = 1+b1+b2)
  // For non-zero Q (resonance), NT5 reduces K by the resonance Q amount.
  // We apply Q resonance gain reduction: −Q_dB * 0.0375 where Q_dB ≈ 20*log10(Q)
  const double Q_dB = (Q > 1.0f) ? 20.0 * std::log10(double(Q)) : 0.0;
  const double qIdx = Q_dB / 1.5;  // NT5: 1.5 dB per Q index step
  const double K    = (1.0 + b1 + b2) * std::pow(10.0, -qIdx * 0.0375);

  // Convert to Q2.30 integer (B1 stored negated = +|b1|)
  cfK  = COEFF(K   * (1LL << 30));
  cfB1 = COEFF(-b1 * (1LL << 30));  // negate to store as positive
  cfB2 = COEFF(b2  * (1LL << 30));
  }

// NT5 Mix16Filtered / Mix16FilteredInterleaved inner loop:
//   lM = MulDiv(lM, cfK, 1<<30) + MulDiv(z1, cfB1, 1<<30) - MulDiv(z2, cfB2, 1<<30)
//   z2 = z1; z1 = lM
int32_t AllPoleFilter::tick(int32_t x) {
  if(!active) return x;
  int32_t y = int32_t(
      ((int64_t)x  * cfK
     + (int64_t)z1 * cfB1
     - (int64_t)z2 * cfB2) >> 30);
  z2 = z1;
  z1 = y;
  return y;
  }

// ─────────────────────────────────────────────────────────────────────────────
// Voice::render — NT5 CVoice::Mix inner loop
// Mix16: pos >> 12 = index, pos & 0xFFF = frac, sample * vfGain >> 13 = output
// ─────────────────────────────────────────────────────────────────────────────

bool Voice::render(float& outL, float& outR, PREL extraPitchCents) {
  if(!active) return false;

  // ── 1. Envelopes & LFOs ────────────────────────────────────────────────────
  STIME nextEG1, nextEG2, nextLFO, nextVib;
  const int32_t eg1Level = eg1.getLevel(sampleTime, &nextEG1);   // 0..1000
  const int32_t eg2Level = eg2.getLevel(sampleTime, &nextEG2);   // 0..1000
  const int32_t lfoLevel = lfo.getLevel(sampleTime, &nextLFO);   // −100..+100
  const int32_t vibLevel = vib.getLevel(sampleTime, &nextVib);   // −100..+100

  // Check voice done
  if(!eg1.isActive(sampleTime)) {
    active = false;
    return false;
    }

  // ── 2. Pitch modulation (PREL → PFRACT ratio) ─────────────────────────────
  // NT5: prPitch = LFO.GetPitch() + EG2.GetPitch() + vibLFO.GetPitch()
  // extraPitchCents: external sources (e.g. DMUS_CURVET_PBCURVE pitch bend) —
  // NT5 adds channel pitch wheel into the same prPitch sum in CMidiDevice::ProcessBuffer.
  PREL pitchMod = PREL(eg2Level)  * eg2ToPitch  / 1000
                + PREL(lfoLevel)  * lfoToPitch  / 100
                + PREL(vibLevel)  * vibToPitch  / 100
                + extraPitchCents;
  // Apply pitch ratio to base pitch increment
  PFRACT curPitch = PFRACT(int64_t(basePitch) * prelToPfract(pitchMod) >> 12);
  if(curPitch < 1) curPitch = 1;

  // ── 3. Sample fetch — NT5 Mix16 inner loop ─────────────────────────────────
  // pos >> 12 = integer part, pos & 0xFFF = 12-bit fractional part
  const uint32_t idx  = uint32_t(samplePos >> 12);
  const uint32_t frac = uint32_t(samplePos & 0xFFF);

  if(idx >= numSamples) {
    // Non-looping sample exhausted while note is still held.
    // Trigger EG release so the voice fades out cleanly instead of cutting abruptly.
    if(!releasing) release();
    if(!eg1.isActive(sampleTime)) { active = false; return false; }
    sampleTime++;
    return true;  // output 0 (outL/outR unchanged) while release runs
    }

  // ── Catmull-Rom Hermite 4-point interpolation ─────────────────────────────
  // Better quality than NT5 2-point linear, especially for samples transposed
  // far below their unity pitch (large downward shift → many output samples per
  // input sample → linear smears harmonic clarity on low notes / percussion).
  //
  // fs(di): fetch sample at index (idx+di) with loop wrap and boundary clamp.
  const int32_t s0 = int32_t(samples[idx]);  // current sample (always valid)
  auto fs = [&](int32_t di) -> int32_t {
    int32_t i = int32_t(idx) + di;
    if(looping && loopEnd > loopStart) {
      const int32_t ls = int32_t(loopStart);
      const int32_t le = int32_t(loopEnd);
      if(i >= le)
        i = ls + (i - le);                      // forward wrap (di = +1 or +2)
      else if(di < 0 && i < ls && int32_t(idx) >= ls)
        i = le + di;                             // backward wrap (di = -1 at loop start)
      }
    if(i < 0) i = 0;
    if(uint32_t(i) >= numSamples) return 0;
    return int32_t(samples[uint32_t(i)]);
    };

  const int32_t sm1 = fs(-1);
  const int32_t s1  = fs(+1);
  const int32_t s2  = fs(+2);

  // Catmull-Rom: p(t) = ((a·t + b)·t + c)·t + s0,  t ∈ [0,1)
  const float t  = float(frac) * (1.0f / 4096.0f);
  const float fa = 0.5f * float(-sm1 + 3*s0 - 3*s1 + s2);
  const float fb = 0.5f * float(2*sm1 - 5*s0 + 4*s1 - s2);
  const float fc = 0.5f * float(s1 - sm1);
  int32_t lM     = int32_t(((fa * t + fb) * t + fc) * t + float(s0));

  // ── 4. Filter (NT5 all-pole DF-I) ─────────────────────────────────────────
  if(filter.active) {
    // Optional: modulate cutoff with EG2/LFO (recompute coefficients if changed)
    // For efficiency: only recompute when modulation is significant
    if(eg2ToFilter != 0 || lfoToFilter != 0) {
      PREL cutMod = PREL(eg2Level) * eg2ToFilter / 1000
                  + PREL(lfoLevel) * lfoToFilter / 100;
      if(std::abs(cutMod) > 10) {
        float newCut = filterCutBase * std::pow(2.f, float(cutMod) / 1200.f);
        filter.setup(newCut, filterQ, outSampleRate);
        }
      }
    lM = filter.tick(lM);
    }

  // ── 5. Volume: EG1 dB-correct → VFRACT, then >>13 (NT5 Mix16) ─────────────
  // NT5 reference: vfGain = VRELToVFRACT(eg1.GetVolume())  (voice.cpp Mix16)
  // eg1Level (0..1000 permille) → VREL = level*96/10 − 9600 → amplitude via table.
  // This is the authentic NT5 curve: at level=500 → −48 dB → 0.4% amplitude
  // (vs. linear 50%). Makes attacks punchier and releases shorter-sounding,
  // reducing average mix energy → less compression → more perceived "body".
  const VFRACT eg1Gain = eg1GainTable()[eg1Level];           // 0..4095 (4095 = 0 dB)
  VFRACT vfL = VFRACT(int64_t(staticL) * eg1Gain / 4095);
  VFRACT vfR = VFRACT(int64_t(staticR) * eg1Gain / 4095);

  // LFO volume modulation (kDstAtten semantics: positive LFO → more attenuation → quieter).
  // vrLFO in VREL, but applied as attenuation so negate before dB conversion.
  if(lfoToVolume != 0) {
    VREL vrLFO = VREL(lfoLevel) * lfoToVolume / 100;
    double dB  = -double(vrLFO) / 100.0; // negate: attenuation is positive = quieter
    double fac = std::pow(10.0, dB / 20.0);
    vfL = VFRACT(vfL * fac);
    vfR = VFRACT(vfR * fac);
    }

  // NT5: lA *= vfLVolume; lA >>= 13; (result is 16-bit range)
  int32_t lA = int32_t(int64_t(lM) * vfL >> 13);
  int32_t lR = int32_t(int64_t(lM) * vfR >> 13);

  // Saturation (NT5 overflow handling)
  if(lA >  32767) lA =  32767;
  if(lA < -32768) lA = -32768;
  if(lR >  32767) lR =  32767;
  if(lR < -32768) lR = -32768;

  // ── 6. Accumulate into float output ────────────────────────────────────────
  outL += float(lA) / 32768.f;
  outR += float(lR) / 32768.f;

  // ── 7. Advance sample position ─────────────────────────────────────────────
  samplePos += curPitch;
  sampleTime++;

  // Loop handling
  if(looping && loopEnd > loopStart) {
    const bool doLoop = !loopSustain || !releasing;
    if(doLoop) {
      while(uint32_t(samplePos >> 12) >= loopEnd)
        samplePos -= PFRACT((loopEnd - loopStart) << 12);
      }
    else if(uint32_t(samplePos >> 12) >= numSamples) {
      active = false;
      return false;
      }
    }
  else {
    if(uint32_t(samplePos >> 12) >= numSamples) {
      active = false;
      return false;
      }
    }

  return true;
  }

void Voice::release() {
  releasing = true;
  eg1.noteOff(sampleTime);
  eg2.noteOff(sampleTime);
  }

void Voice::kill() {
  active    = false;
  releasing = false;
  }

// ─────────────────────────────────────────────────────────────────────────────
// CompiledInstr::findRegion — NT5 CInstrument::ScanForRegion (verbatim logic)
// ─────────────────────────────────────────────────────────────────────────────
const CompiledRegion* CompiledInstr::findRegion(uint8_t key, uint8_t vel) const {
  for(const auto& r : regions)
    if(key >= r.keyLo && key <= r.keyHi && vel >= r.velLo && vel <= r.velHi)
      return &r;
  return nullptr;
  }

// ─────────────────────────────────────────────────────────────────────────────
// Synth — polyphonic voice pool
// ─────────────────────────────────────────────────────────────────────────────

Synth::Synth(std::shared_ptr<WaveBank> bank, CompiledInstr instr, int outSampleRate)
  : bank_(std::move(bank)), instr_(std::move(instr)), outRate_(outSampleRate) {
  }

Voice* Synth::allocVoice() {
  // 1. First free slot
  for(auto& v : voices_)
    if(!v.active)
      return &v;

  // 2. Steal the releasing voice furthest into its release (quietest).
  //    We use (sampleTime - eg1.stopTime) as release age: larger = further along = quieter.
  Voice* victim = nullptr;
  STIME  maxAge  = -1;
  for(auto& v : voices_) {
    if(!v.releasing) continue;
    const STIME age = v.sampleTime - v.eg1.stopTime;
    if(!victim || age > maxAge) {
      victim = &v;
      maxAge  = age;
      }
    }

  // 3. Fallback: steal the oldest active voice (created earliest = most advanced).
  if(!victim) {
    for(auto& v : voices_) {
      if(!victim || v.birthTime < victim->birthTime)
        victim = &v;
      }
    }

  if(victim) victim->kill();
  return victim;
  }

void Synth::killGroup(uint16_t group) {
  if(group == 0) return;
  // Trigger release instead of instant kill so the EG fades the voice out —
  // prevents clicks when switching notes on monophonic (keyGroup) instruments.
  for(auto& v : voices_)
    if(v.active && !v.releasing && v.keyGroup == group)
      v.release();
  }

void Synth::noteOn(uint8_t note, uint8_t vel) {
  if(vel == 0) { noteOff(note); return; }

  const CompiledRegion* reg = instr_.findRegion(note, vel);
  if(!reg) {
    Log::d("DxSynth: no region for note=", int(note), " vel=", int(vel));
    return;
    }
  if(!reg->samples || reg->numSamples == 0) {
    Log::d("DxSynth: region has no samples for note=", int(note));
    return;
    }

  if(reg->keyGroup) killGroup(reg->keyGroup);

  Voice* v = allocVoice();
  if(!v) return;

  v->active      = true;
  v->releasing   = false;
  v->note        = note;
  v->vel         = vel;
  v->keyGroup    = reg->keyGroup;
  v->samples     = reg->samples;
  v->numSamples  = reg->numSamples;
  v->looping     = reg->looping;
  v->loopSustain = reg->loopSustain;
  v->loopStart   = reg->loopStart;
  v->loopEnd     = reg->loopEnd;
  v->samplePos   = 0;
  v->sampleTime    = globalTime_;
  v->birthTime     = globalTime_;
  v->outSampleRate = outRate_;

  // ── Pitch: basePitch in PFRACT (4096 = 1.0 sample-per-output-sample) ───────
  // noteCents = (note − unityNote) * scaleTuning + fineTune
  const float noteCents = float(int(note) - int(reg->unityNote)) * reg->art.scaleTuning
                        + float(reg->fineTuneCents);
  // PFRACT = (sampleRate / outRate) * 2^(cents/1200) * 4096
  const double pitchRatio = double(reg->sampleRate) / double(outRate_)
                          * std::pow(2.0, noteCents / 1200.0);
  v->basePitch = PFRACT(pitchRatio * 4096.0 + 0.5);
  if(v->basePitch < 1) v->basePitch = 1;

  // ── Static gain (attenuation + velocity) → VFRACT ──────────────────────────
  // NT5: static gain = VRELToVFRACT(attenuation_VREL + velocity_VREL)
  // attenDB positive = quieter; velocity 0..127 → dB
  float totalAttendB = reg->attenuationDB;                         // wave-level (lAttenuation)
  totalAttendB      += reg->art.attenuationDB;                     // connection-block static atten
  // DLS velocity→attenuation (kSrcVelocity→kDstAtten): extra quiet-ness at low velocities.
  // velToAtten stores the connection lScale (centibels at vel=0); linearly interpolate to vel=127→0.
  if(reg->art.velToAtten != 0.f)
    totalAttendB += reg->art.velToAtten * float(127 - int(vel)) / 127.f;
  float velDB        = 20.f * std::log10(std::max(1.f/127.f, float(vel) / 127.f)); // velocity → dB (≤0)
  float staticDB     = -(totalAttendB) + velDB;                    // net linear dB from 0
  VFRACT vfBase      = vrelToVfract(VREL(staticDB * 100.f));       // 100 VREL = 1 dB

  // ── Pan → split into L/R VFRACT (NT5 balance pan) ─────────────────────────
  // NT5 balance pan: centre = full gain on both channels.
  // Positive pan → right (attenuate L); negative → left (attenuate R).
  // Range: −1.0 (full left) .. 0.0 (centre) .. +1.0 (full right).
  float pan   = std::max(-1.0f, std::min(1.0f, pan_ + reg->art.panValue));
  float gainL = std::min(1.0f, 1.0f - std::max(0.f, pan));
  float gainR = std::min(1.0f, 1.0f + std::min(0.f, pan));
  v->staticL  = VFRACT(int32_t(float(vfBase) * gainL));
  v->staticR  = VFRACT(int32_t(float(vfBase) * gainR));

  // ── Modulation routing ─────────────────────────────────────────────────────
  v->eg2ToPitch  = PREL(reg->art.eg2ToPitch);
  v->eg2ToFilter = PREL(reg->art.eg2ToFilter);
  v->lfoToPitch  = PREL(reg->art.lfoToPitch);
  v->lfoToVolume = VREL(reg->art.lfoToVolume);
  v->lfoToFilter = PREL(reg->art.lfoToFilter);
  v->vibToPitch  = PREL(reg->art.vibToPitch);

  // ── Filter ─────────────────────────────────────────────────────────────────
  v->filterCutBase = reg->art.filterCutHz;
  v->filterQ       = std::max(0.5f, reg->art.filterQ);
  const bool wantFilter = (reg->art.filterCutHz < float(outRate_) * 0.47f
                           && reg->art.filterCutHz > 20.f);
  if(wantFilter)
    v->filter.setup(reg->art.filterCutHz, v->filterQ, outRate_);
  else
    v->filter.disable();

  // ── Envelopes ──────────────────────────────────────────────────────────────
  v->eg1.setup(reg->art, note, vel, true,  outRate_);
  v->eg2.setup(reg->art, note, vel, false, outRate_);
  v->eg1.noteOn(globalTime_);
  v->eg2.noteOn(globalTime_);

  // ── LFOs ───────────────────────────────────────────────────────────────────
  v->lfo.init(reg->art.lfoFreqHz, reg->art.lfoDelaySec, outRate_, globalTime_);
  v->vib.init(reg->art.vibFreqHz, reg->art.vibDelaySec, outRate_, globalTime_);

  // Runtime diagnostic: log modulation params for every note so bayan-like
  // instruments can be identified by note + lfoToPitch / vibToPitch values.
  if(v->lfoToPitch != 0 || v->vibToPitch != 0 || v->lfoToVolume != 0) {
    char buf[160];
    std::snprintf(buf, sizeof(buf),
      "DxSynth noteOn n=%d v=%d  lfoP=%dct vibP=%dct lfoVol=%d"
      "  lfoF=%.1fHz vibF=%.1fHz  lfoDelay=%.3fs vibDelay=%.3fs",
      int(note), int(vel),
      int(v->lfoToPitch), int(v->vibToPitch), int(v->lfoToVolume),
      reg->art.lfoFreqHz, reg->art.vibFreqHz,
      reg->art.lfoDelaySec, reg->art.vibDelaySec);
    Log::d(buf);
    }
  }

void Synth::noteOff(uint8_t note) {
  for(auto& v : voices_)
    if(v.active && !v.releasing && v.note == note)
      v.release();
  }

void Synth::setPan(float pan) {
  // Input: 0..1 (0.5 = centre). Convert to −1..+1 for balance-pan arithmetic.
  pan_ = std::max(-1.0f, std::min(1.0f, (pan - 0.5f) * 2.0f));
  }

void Synth::setPitchBend(uint16_t raw14) {
  // 14-bit MIDI PB: 0..16383, centre=8192. Re-centre then scale to cents.
  const int32_t signed14 = int32_t(raw14) - 8192;
  pitchBendCents_ = (float(signed14) / 8192.f) * pitchBendRangeCents_;
  }

void Synth::setPitchBendNormalized(float n) {
  if(n >  1.f) n =  1.f;
  if(n < -1.f) n = -1.f;
  pitchBendCents_ = n * pitchBendRangeCents_;
  }

bool Synth::hasNotes() const {
  for(const auto& v : voices_)
    if(v.active) return true;
  return false;
  }

void Synth::render(float* buf, int frames) {
  const PREL pbCents = PREL(std::lround(pitchBendCents_));
  for(auto& v : voices_) {
    if(!v.active) continue;
    float* p = buf;
    for(int i = 0; i < frames; ++i, p += 2) {
      if(!v.render(p[0], p[1], pbCents))
        break;
      }
    }
  globalTime_ += frames;
  }

// ─────────────────────────────────────────────────────────────────────────────
// WaveBank::build — decode all DLS waves to int16 mono (NT5: m_pnWave = short*)
// ─────────────────────────────────────────────────────────────────────────────
std::shared_ptr<WaveBank> WaveBank::build(const std::vector<Wave>& waves) {
  auto bank = std::make_shared<WaveBank>();
  const size_t n = waves.size();
  bank->offsets.resize(n);
  bank->lengths.resize(n);
  bank->sampleRates.resize(n);
  bank->meta.resize(n);

  uint32_t total = 0;
  for(size_t i = 0; i < n; ++i) {
    const auto& w  = waves[i];
    const uint32_t ch     = std::max(uint16_t(1), w.wfmt.wChannels);
    const uint32_t bits   = w.wfmt.wBitsPerSample ? w.wfmt.wBitsPerSample : 16u;
    const uint32_t bps    = (bits + 7u) / 8u;
    const uint32_t frames = bps > 0 ? uint32_t(w.wavedata.size()) / (ch * bps) : 0u;
    bank->offsets[i]     = total;
    bank->lengths[i]     = frames;
    bank->sampleRates[i] = w.wfmt.dwSamplesPerSec ? w.wfmt.dwSamplesPerSec : 22050u;
    total += frames;

    // Capture Wave metadata before the caller may clear the wave vector
    WaveMeta& m       = bank->meta[i];
    m.cbSize          = w.waveSample.cbSize;
    m.unityNote       = w.waveSample.usUnityNote;
    m.fineTune        = w.waveSample.sFineTune;
    m.lAttenuation    = w.waveSample.lAttenuation;
    if(!w.loop.empty()) {
      m.hasLoop    = true;
      m.loopType   = w.loop[0].ulLoopType;
      m.loopStart  = w.loop[0].ulLoopStart;
      m.loopLength = w.loop[0].ulLoopLength;
      }
    }

  bank->pcm.resize(total, 0);

  for(size_t i = 0; i < n; ++i) {
    const auto& w      = waves[i];
    const uint32_t ch  = std::max(uint16_t(1), w.wfmt.wChannels);
    const uint32_t bits = w.wfmt.wBitsPerSample ? w.wfmt.wBitsPerSample : 16u;
    const uint32_t frames = bank->lengths[i];
    if(frames == 0 || w.wavedata.empty()) continue;

    int16_t* dst = bank->pcm.data() + bank->offsets[i];

    if(bits == 16) {
      const int16_t* src = reinterpret_cast<const int16_t*>(w.wavedata.data());
      if(ch == 1) {
        for(uint32_t f = 0; f < frames; ++f)
          dst[f] = src[f];
        }
      else {
        // Stereo → mono: mix L+R / 2
        for(uint32_t f = 0; f < frames; ++f)
          dst[f] = int16_t((int32_t(src[f*ch]) + int32_t(src[f*ch+1])) / 2);
        }
      }
    else if(bits == 8) {
      // 8-bit unsigned PCM → int16
      const uint8_t* src = reinterpret_cast<const uint8_t*>(w.wavedata.data());
      if(ch == 1) {
        for(uint32_t f = 0; f < frames; ++f)
          dst[f] = int16_t((int32_t(src[f]) - 128) * 256);
        }
      else {
        for(uint32_t f = 0; f < frames; ++f) {
          int32_t s = (int32_t(src[f*ch]) + int32_t(src[f*ch+1])) / 2 - 128;
          dst[f] = int16_t(s * 256);
          }
        }
      }
    // else: unsupported bit depth, leave as zeros
    }

  // ── Loop crossfade — anti-click at loop boundaries ───────────────────────────
  // Blend the last kFadeLen samples before loopEnd toward the corresponding
  // samples at loopStart, so the hard position-wrap introduces no discontinuity.
  // 8 samples @ 44100 Hz = 0.18 ms — inaudible in normal playback.
  for(size_t i = 0; i < n; ++i) {
    const WaveMeta& m = bank->meta[i];
    if(!m.hasLoop || m.loopLength == 0) continue;
    const uint32_t loopEnd = m.loopStart + m.loopLength;
    if(loopEnd > bank->lengths[i] || m.loopStart >= loopEnd) continue;

    int16_t* data = bank->pcm.data() + bank->offsets[i];

    // Fade window: at most 8 samples, or 1/4 of loop length, whichever is smaller.
    const int32_t kFadeLen = int32_t(std::min(uint32_t(8), m.loopLength / 4));
    if(kFadeLen > 0) {
      // Blend data[loopEnd-kFadeLen .. loopEnd-1] toward data[loopStart].
      // At j=kFadeLen-1: t=1.0 → data[loopEnd-1] == data[loopStart] exactly.
      // This eliminates the discontinuity at the hard position-wrap, since the
      // linear interpolation in render() uses samples[loopStart] as s1 when
      // idx+1 wraps past loopEnd — which now matches data[loopEnd-1] perfectly.
      const int32_t tgt = int32_t(data[m.loopStart]);
      for(int32_t j = 0; j < kFadeLen; ++j) {
        const int32_t endIdx = int32_t(loopEnd) - kFadeLen + j;
        if(endIdx < 0 || uint32_t(endIdx) >= bank->lengths[i]) continue;
        const float   t   = float(j + 1) / float(kFadeLen);  // 0..1, reaches 1 at loopEnd-1
        const int32_t src = int32_t(data[endIdx]);
        data[endIdx] = int16_t(src + int32_t(float(tgt - src) * t));
        }
      }
    }

  return bank;
  }

// ─────────────────────────────────────────────────────────────────────────────
// Articulation parser — DLS ConnectionBlocks → ArticParams (physical units)
// ─────────────────────────────────────────────────────────────────────────────
static void parseConnectionBlock(const DlsCollection::ConnectionBlock& cb,
                                  ArticParams& art) {
  if(cb.usControl != 0) return; // skip controller-modulated connections

  const int32_t raw  = cb.lScale >> 16;  // Q16.16 → integer part
  const float   rawF = float(raw);

  // Absolute pitch cents → Hz
  auto ac2hz = [](float ac) { return 440.f * std::pow(2.f, (ac - 6900.f) / 1200.f); };

  switch(cb.usSource) {
    case kSrcNone:
      switch(cb.usDestination) {
        case kDstAtten:    art.attenuationDB  += rawF * 0.1f; break;   // centibels → dB
        case kDstPan:      art.panValue       += rawF / 500.f;  break; // −500..+500 → −1.0..+1.0
        case kDstEG1Att:   art.eg1Attack       = tc2sec(rawF); break;
        case kDstEG1Dec:   art.eg1Decay        = tc2sec(rawF); break;
        case kDstEG1Rel:   art.eg1Release      = tc2sec(rawF); break;
        // EG sustain: DLS CONN_DST_EG*_SUSTAIN is attenuation in centibels (0 = 0 dB = full sustain).
        // Convert: sustainLinear = 10^(−centibels/200).  raw=0→1.0, raw=1000→≈0.
        case kDstEG1Sus: { float cB = float(std::max(0, std::min(1440, raw)));
                           art.eg1Sustain = std::pow(10.f, -cB / 200.f); break; }
        case kDstEG1Dly:   art.eg1Delay        = tc2sec(rawF); break;
        case kDstEG1Hold:  art.eg1Hold         = tc2sec(rawF); break;
        case kDstEG2Att:   art.eg2Attack       = tc2sec(rawF); break;
        case kDstEG2Dec:   art.eg2Decay        = tc2sec(rawF); break;
        case kDstEG2Rel:   art.eg2Release      = tc2sec(rawF); break;
        case kDstEG2Sus: { float cB = float(std::max(0, std::min(1440, raw)));
                           art.eg2Sustain = std::pow(10.f, -cB / 200.f); break; }
        case kDstEG2Dly:   art.eg2Delay        = tc2sec(rawF); break;
        case kDstEG2Hold:  art.eg2Hold         = tc2sec(rawF); break;
        case kDstFiltCut:  art.filterCutHz     = ac2hz(rawF); break;
        case kDstFiltQ:    art.filterQ         = std::max(0.5f, std::pow(10.f, rawF / 200.f)); break;
        case kDstLFOFreq:  art.lfoFreqHz       = std::max(0.01f, ac2hz(rawF)); break;
        case kDstLFODelay: art.lfoDelaySec     = std::max(0.f, tc2sec(rawF)); break;
        case kDstVibFreq:  art.vibFreqHz       = std::max(0.01f, ac2hz(rawF)); break;
        case kDstVibDelay: art.vibDelaySec     = std::max(0.f, tc2sec(rawF)); break;
        default: break;
        }
      break;

    case kSrcLFO:
      switch(cb.usDestination) {
        case kDstPitch:    art.lfoToPitch   = rawF;         break;  // cents at full LFO
        case kDstAtten:    art.lfoToVolume  = rawF * 10.f;  break;  // cB → VREL (1 cB = 10 VREL)
        case kDstFiltCut:  art.lfoToFilter  = rawF;         break;
        default: break;
        }
      break;

    case kSrcVibrato:
      if(cb.usDestination == kDstPitch)
        art.vibToPitch = rawF;
      break;

    case kSrcEG2:
      switch(cb.usDestination) {
        case kDstPitch:   art.eg2ToPitch   = rawF; break;
        case kDstFiltCut: art.eg2ToFilter  = rawF; break;
        default: break;
        }
      break;

    case kSrcKeyNum:
      switch(cb.usDestination) {
        case kDstPitch:
          // NT5: scaleTuning = lScale/12800 * 100 cents/semitone
          art.scaleTuning   = float(cb.lScale) / 12800.f * 100.f;
          break;
        case kDstEG1Hold:  art.eg1KeyToHold  = rawF; break;
        case kDstEG1Dec:   art.eg1KeyToDecay = rawF; break;
        case kDstEG2Hold:  art.eg2KeyToHold  = rawF; break;
        case kDstEG2Dec:   art.eg2KeyToDecay = rawF; break;
        default: break;
        }
      break;

    case kSrcVelocity:
      switch(cb.usDestination) {
        case kDstEG1Att:   art.eg1VelAttack = rawF; break;
        case kDstEG2Att:   art.eg2VelAttack = rawF; break;
        // DLS default: velocity linearly attenuates volume.
        // lScale (Q16.16) = total centibels of attenuation when vel=0.
        // At noteOn: extra_atten = velToAtten * (127 - vel) / 127  centibels.
        case kDstAtten:    art.velToAtten   = float(cb.lScale >> 16) * 0.1f; break; // cB at vel=0
        default: break;
        }
      break;

    default: break;
    }
  }

// ─────────────────────────────────────────────────────────────────────────────
// findInstrumentInVec — replicates DlsCollection::findInstrument on a stored copy
// Searches the instrument vector with the same multi-pass priority as NT5:
//   1. Exact bank+instrument match
//   2. Single-byte bank fallback
//   3. Program-only last resort
// ─────────────────────────────────────────────────────────────────────────────
static const DlsCollection::Instrument*
findInstrumentInVec(const std::vector<DlsCollection::Instrument>& instruments,
                    uint32_t dwPatch) {
  const uint8_t  bankHi     = uint8_t((dwPatch & 0x00FF0000u) >> 16);
  const uint8_t  bankLo     = uint8_t((dwPatch & 0x0000FF00u) >>  8);
  const uint8_t  patch      = uint8_t( dwPatch & 0x000000FFu);
  const uint32_t legacyBank = (uint32_t(bankHi) << 16) + uint32_t(bankLo);

  // Pass 1: exact bank match
  for(size_t idx = instruments.size(); idx > 0; --idx) {
    const auto& i = instruments[idx-1];
    if(uint8_t(i.header.Locale.ulInstrument & 0xFFu) != patch) continue;
    const uint32_t instrBankRaw = i.header.Locale.ulBank;
    const uint8_t  instrBankHi  = uint8_t((instrBankRaw >> 16) & 0xFFu);
    const uint8_t  instrBankLo  = uint8_t( instrBankRaw        & 0xFFu);
    if(instrBankRaw == legacyBank || (instrBankHi == bankHi && instrBankLo == bankLo))
      return &i;
    }
  // Pass 2: single-byte bank
  for(size_t idx = instruments.size(); idx > 0; --idx) {
    const auto& i = instruments[idx-1];
    if(uint8_t(i.header.Locale.ulInstrument & 0xFFu) != patch) continue;
    if(uint8_t(i.header.Locale.ulBank & 0x7Fu) == uint8_t(bankLo & 0x7Fu))
      return &i;
    }
  // Pass 3: program only
  for(size_t idx = instruments.size(); idx > 0; --idx) {
    const auto& i = instruments[idx-1];
    if(uint8_t(i.header.Locale.ulInstrument & 0xFFu) == patch)
      return &i;
    }
  return nullptr;
  }

// ─────────────────────────────────────────────────────────────────────────────
// compileInstr — NT5 CInstrument::Download / CSourceRegion setup equivalent
//
// Takes a pre-copied instruments vector (lifetime-safe: no raw ptr to DlsCollection)
// and a WaveBank with embedded WaveMeta (captured before wave.clear()).
// ─────────────────────────────────────────────────────────────────────────────
CompiledInstr DxSynth::compileInstr(
    const std::vector<DlsCollection::Instrument>& instruments,
    uint32_t                                       dwPatch,
    const std::shared_ptr<WaveBank>&               bank) {

  char patchHex[12];
  std::snprintf(patchHex, sizeof(patchHex), "%08X", dwPatch);

  const DlsCollection::Instrument* instr = findInstrumentInVec(instruments, dwPatch);
  if(!instr) {
    Log::d("DxSynth: patch 0x", patchHex, " not found, using first instrument");
    if(!instruments.empty())
      instr = &instruments[0];
    else
      return {};
    }

#ifndef NDEBUG
  Log::d("DxSynth: compiling patch 0x", patchHex,
         " regions=", (uint32_t)instr->regions.size());
#endif

  CompiledInstr result;

  for(const auto& reg : instr->regions) {
    const uint32_t waveIdx = reg.wlink.ulTableIndex;
    if(waveIdx >= bank->lengths.size()) continue;
    if(bank->lengths[waveIdx] == 0)     continue;

    CompiledRegion cr;

    // Key / velocity ranges
    cr.keyLo = uint8_t(reg.head.RangeKey.usLow);
    cr.keyHi = uint8_t(reg.head.RangeKey.usHigh);
    if(cr.keyHi < cr.keyLo) continue;

    const bool badVel  = reg.head.RangeVelocity.usHigh < reg.head.RangeVelocity.usLow;
    const bool zeroVel = reg.head.RangeVelocity.usLow == 0
                      && reg.head.RangeVelocity.usHigh == 0;
    if(badVel || zeroVel) {
      cr.velLo = 0; cr.velHi = 127;
      }
    else {
      cr.velLo = uint8_t(reg.head.RangeVelocity.usLow);
      cr.velHi = uint8_t(reg.head.RangeVelocity.usHigh);
      }
    cr.keyGroup = reg.head.usKeyGroup;

    // ── Wave sample parameters (region overrides wave metadata) ───────────
    // bank->meta[] was captured from Wave::WaveSample before wave.clear().
    const WaveBank::WaveMeta& wm = bank->meta[waveIdx];
    uint16_t unityNote        = reg.waveSample.usUnityNote;
    int16_t  fineTune         = reg.waveSample.sFineTune;
    int32_t  lAttenuation     = reg.waveSample.lAttenuation;

    if(reg.waveSample.cbSize == 0 && wm.cbSize != 0) {
      // Region has no own waveSample — use the one from the wave itself
      unityNote    = wm.unityNote;
      fineTune     = wm.fineTune;
      lAttenuation = wm.lAttenuation;
      }
    if(unityNote == 0) unityNote = 60;   // default: middle C

    cr.unityNote     = uint8_t(std::min(127, int(unityNote)));
    cr.fineTuneCents = fineTune;
    // DLS WSMPL lAttenuation: plain int32 in centibels (1 cB = 0.1 dB). Positive = quieter.
    // Do NOT treat as Q16.16 (that's only for connection-block lScale).
    cr.attenuationDB = std::max(0.f, float(lAttenuation) * 0.1f);

    // ── Sample pointer into WaveBank::pcm ────────────────────────────────
    cr.samples    = bank->pcm.data() + bank->offsets[waveIdx];
    cr.numSamples = bank->lengths[waveIdx];
    cr.sampleRate = bank->sampleRates[waveIdx];

    // ── Loop — region loop takes priority; fall back to wave metadata ─────
    if(!reg.loop.empty() && reg.loop[0].ulLoopLength > 0) {
      cr.looping     = true;
      cr.loopSustain = (reg.loop[0].ulLoopType == 1);
      cr.loopStart   = reg.loop[0].ulLoopStart;
      cr.loopEnd     = reg.loop[0].ulLoopStart + reg.loop[0].ulLoopLength;
      }
    else if(wm.hasLoop && wm.loopLength > 0) {
      cr.looping     = true;
      cr.loopSustain = (wm.loopType == 1);
      cr.loopStart   = wm.loopStart;
      cr.loopEnd     = wm.loopStart + wm.loopLength;
      }
    if(cr.looping) {
      cr.loopEnd = std::min(cr.loopEnd, cr.numSamples);
      if(cr.loopEnd <= cr.loopStart) cr.looping = false;
      }

    // ── Articulation ──────────────────────────────────────────────────────
    if(!reg.articulationConnections.empty()) {
      for(const auto& c : reg.articulationConnections)
        parseConnectionBlock(c, cr.art);
      }
    else {
      for(const auto& art : instr->articulators)
        for(const auto& c : art.connectionBlocks)
          parseConnectionBlock(c, cr.art);
      }

#ifndef NDEBUG
    // ── Articulation diagnostics ───────────────────────────────────────────
    // Per-region dump is useful while tuning the parser against NT5 behaviour,
    // but produces dozens of lines per patch (Gothic has ≈60 patches with up
    // to 63 regions each → ~3000 lines at boot). Gate to debug builds only.
    {
      char artBuf[320];
      std::snprintf(artBuf, sizeof(artBuf),
        "  DxSynth art: wave=%u"
        "  eg1: att=%.4fs dcy=%.3fs sus=%.3f rel=%.3fs"
        "  eg2: att=%.4fs dcy=%.3fs sus=%.3f rel=%.3fs"
        "  velAtten=%.1fcB  waveAtten=%.2fdB  cbAtten=%.2fdB"
        "  lfoP=%.1fct lfoVol=%.0fVREL  vibP=%.1fct"
        "  scaleTune=%.1f  cut=%.0fHz",
        waveIdx,
        cr.art.eg1Attack, cr.art.eg1Decay, cr.art.eg1Sustain, cr.art.eg1Release,
        cr.art.eg2Attack, cr.art.eg2Decay, cr.art.eg2Sustain, cr.art.eg2Release,
        cr.art.velToAtten, cr.attenuationDB, cr.art.attenuationDB,
        cr.art.lfoToPitch, cr.art.lfoToVolume, cr.art.vibToPitch,
        cr.art.scaleTuning, cr.art.filterCutHz);
      Log::d(artBuf);
      }
#endif

    result.regions.push_back(cr);
    }

#ifndef NDEBUG
  Log::d("DxSynth: compiled ", result.regions.size(), " regions");
#endif
  return result;
  }
