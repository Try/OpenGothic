#include "nt5reverb.h"

#ifndef OPENGOTHIC_WITH_NT5_DMSYNTH
// Flag off → NT5 sources (sverb.cpp) are not in the build; emit empty
// method definitions so the translation unit links. Mixer code should
// never instantiate NT5Reverb in this configuration (it uses FreeVerb).
namespace Dx8 {
NT5Reverb::NT5Reverb()  = default;
NT5Reverb::~NT5Reverb() = default;
void NT5Reverb::init(uint32_t)                                {}
void NT5Reverb::reconfigure()                                 {}
void NT5Reverb::setParams(float,float,float,float)            {}
void NT5Reverb::setWet(float)                                 {}
void NT5Reverb::setRoomSize(float)                            {}
void NT5Reverb::setDamping(float)                             {}
void NT5Reverb::mute()                                        {}
void NT5Reverb::process(float*, size_t)                       {}
}
#else

// sverb has been ported from `long` to `int32_t` (see port note at the
// head of sverb.h), so the state-buffer layout is the same on every
// platform: the first two int32_t slots are the ring indices, the rest
// is the float delay line. `sizeof(int32_t) == sizeof(float) == 4` by
// standard, so the reinterpret casts below are layout-compatible.

#include "nt5/sverb.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Dx8 {

NT5Reverb::NT5Reverb()  = default;
NT5Reverb::~NT5Reverb() = default;

void NT5Reverb::init(uint32_t sampleRate) {
  sampleRate_ = static_cast<float>(sampleRate);

  // Allocate the coefficient blob and the state ring buffer, both sized
  // by the engine itself (matches whatever sCoefsStruct / delay lengths
  // sverb.h is currently compiled with).
  coefs_.assign(static_cast<size_t>(GetCoefsSize()), 0u);
  states_.assign(static_cast<size_t>(GetStatesSize()), 0u);

  // InitSVerb stamps size/version/SampleRate into the coef blob and wires
  // up the sample-rate-dependent delay lengths. Must be called before
  // SetSVerb so the clear-to-dry conversions have a valid rate.
  ::InitSVerb(sampleRate_, coefs_.data());
  ::InitSVerbStates(reinterpret_cast<int32_t*>(states_.data()));

  initialized_ = true;
  dirty_       = true;        // push initial parameters before first process()
  reconfigure();
  }

void NT5Reverb::reconfigure() {
  if(!initialized_ || !dirty_)
    return;
  // NT5 clamps hfRatio into a safe range internally, but values at exactly
  // 0 or 1 cause divide-by-zero / log(0); keep a small margin.
  const float hf = std::clamp(hfRatio_, 0.001f, 0.999f);
  ::SetSVerb(inGainDb_, revMixDb_, revTimeMs_, hf, coefs_.data());
  dirty_ = false;
  }

void NT5Reverb::setParams(float inGainDb, float revMixDb, float revTimeMs, float hfRatio) {
  inGainDb_  = inGainDb;
  revMixDb_  = revMixDb;
  revTimeMs_ = revTimeMs;
  hfRatio_   = hfRatio;
  dirty_     = true;
  }

void NT5Reverb::setWet(float v) {
  // Map linear wet [0..1] to dRevMix in dB.
  //   0.0 → -96 dB  (effectively dry-only)
  //   1.0 →   0 dB  (fully wet)
  v = std::clamp(v, 0.f, 1.f);
  if(v <= 0.0001f)
    revMixDb_ = -96.f;
  else
    revMixDb_ = 20.f * std::log10(v);
  dirty_ = true;
  }

void NT5Reverb::setRoomSize(float v) {
  // FreeVerb's room-size knob is 0..1 controlling feedback; SVerb uses
  // RT60 in ms. Map linearly: 0 → 0.5 s, 1 → 3.0 s. Covers I3DL2 presets
  // from "Small Room" (0.4 s) through "Music" (1.49 s) up to "Cave" (2.9 s).
  v = std::clamp(v, 0.f, 1.f);
  revTimeMs_ = 500.f + v * (3000.f - 500.f);
  dirty_     = true;
  }

void NT5Reverb::setDamping(float v) {
  // FreeVerb damping 0..1: 0 = bright, 1 = dark.
  // SVerb hfRatio:         0 = dark,  1 = bright (ratio of HF-RT60 to RT60).
  // So invert: hf = 1 - v, with guard-band to avoid ratio==0/1 extremes.
  v = std::clamp(v, 0.f, 1.f);
  hfRatio_ = std::clamp(1.f - v, 0.001f, 0.999f);
  dirty_   = true;
  }

void NT5Reverb::mute() {
  if(!initialized_)
    return;
  ::InitSVerbStates(reinterpret_cast<int32_t*>(states_.data()));
  }

void NT5Reverb::process(float* io, size_t frames) {
  if(!enabled_ || !initialized_ || frames == 0)
    return;
  reconfigure();

  // SVerbStereoToStereoFloat is in-place safe: every sample's In1/In2 are
  // read into locals before Out1/Out2 are written to the same slots.
  ::SVerbStereoToStereoFloat(
      static_cast<int32_t>(frames), io, io,
      coefs_.data(),
      reinterpret_cast<float*>(states_.data()));

  if(dryMul_ != 1.f) {
    const size_t n = frames * 2;
    for(size_t i = 0; i < n; ++i)
      io[i] *= dryMul_;
    }
  }

} // namespace Dx8

#endif // OPENGOTHIC_WITH_NT5_DMSYNTH
