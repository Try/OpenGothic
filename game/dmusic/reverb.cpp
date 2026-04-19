#include "reverb.h"

#include <algorithm>
#include <cmath>

using namespace Dx8;

// ── FreeVerb tunings (designed for 44.1 kHz) ─────────────────────────────────
// Jezar Wakefield's public-domain constants. The tuning was tuned by ear at
// 44.1 kHz; for other rates we scale proportionally.
static constexpr int kCombTunings44k[8]    = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static constexpr int kAllpassTunings44k[4] = { 556,  441,  341,  225 };
static constexpr int kStereoSpread44k      = 23;  // R-channel delay offset

static inline int scaleToRate(int samples44k, uint32_t sr) {
  if(sr == 0) return samples44k;
  return int(std::llround(double(samples44k) * double(sr) / 44100.0));
  }

float Reverb::Comb::tick(float in) {
  float out = buf[idx];
  // Small DC blocker / undenormalize by biasing; FreeVerb originally did `+ veryDenormal`.
  // Undenormalize via branchless flush.
  if(std::fabs(out) < 1e-20f) out = 0.f;
  // One-pole lowpass (damping) inside the comb loop — darkens the tail.
  filterst = out*damp2 + filterst*damp1;
  if(std::fabs(filterst) < 1e-20f) filterst = 0.f;
  buf[idx] = in + filterst*feedback;
  ++idx;
  if(idx >= buf.size()) idx = 0;
  return out;
  }

float Reverb::Allpass::tick(float in) {
  float bufout = buf[idx];
  if(std::fabs(bufout) < 1e-20f) bufout = 0.f;
  float out = -in + bufout;
  buf[idx]  = in + bufout*feedback;
  ++idx;
  if(idx >= buf.size()) idx = 0;
  return out;
  }

Reverb::Reverb() = default;

void Reverb::init(uint32_t sampleRate) {
  sampleRate_ = sampleRate;
  for(int i=0; i<kNumCombs; ++i) {
    const int lenL = scaleToRate(kCombTunings44k[i], sampleRate);
    const int lenR = lenL + scaleToRate(kStereoSpread44k, sampleRate);
    combL_[i].resize(size_t(std::max(1,lenL)));
    combR_[i].resize(size_t(std::max(1,lenR)));
    }
  for(int i=0; i<kNumAllpasses; ++i) {
    const int lenL = scaleToRate(kAllpassTunings44k[i], sampleRate);
    const int lenR = lenL + scaleToRate(kStereoSpread44k, sampleRate);
    apL_[i].resize(size_t(std::max(1,lenL)));
    apR_[i].resize(size_t(std::max(1,lenR)));
    apL_[i].feedback = 0.5f;
    apR_[i].feedback = 0.5f;
    }
  applyParams();
  }

void Reverb::setRoomSize(float v) { roomSize_ = std::clamp(v, 0.f, 1.f); applyParams(); }
void Reverb::setDamping (float v) { damping_  = std::clamp(v, 0.f, 1.f); applyParams(); }
void Reverb::setWidth   (float v) { width_    = std::clamp(v, 0.f, 1.f); applyParams(); }
void Reverb::setWet     (float v) { wet_      = std::clamp(v, 0.f, 1.f); applyParams(); }
void Reverb::setDry     (float v) { dry_      = std::clamp(v, 0.f, 1.f); }

void Reverb::applyParams() {
  const float feedback = roomSize_*kScaleRoom + kOffsetRoom;
  const float damp1    = damping_*kScaleDamp;
  const float damp2    = 1.f - damp1;
  for(int i=0; i<kNumCombs; ++i) {
    combL_[i].feedback = feedback;  combR_[i].feedback = feedback;
    combL_[i].damp1 = damp1;        combL_[i].damp2 = damp2;
    combR_[i].damp1 = damp1;        combR_[i].damp2 = damp2;
    }
  // Stereo spread: wet1 is same-side, wet2 is cross-side.
  // width=1 → full stereo; width=0 → mono wet.
  const float scaledWet = wet_ * 3.f;  // FreeVerb scalewet (≈3) — tuned by ear.
  wet1_ = scaledWet * (width_*0.5f + 0.5f);
  wet2_ = scaledWet * ((1.f - width_)*0.5f);
  }

void Reverb::mute() {
  for(int i=0; i<kNumCombs; ++i)    { combL_[i].mute(); combR_[i].mute(); }
  for(int i=0; i<kNumAllpasses; ++i) { apL_[i].mute();  apR_[i].mute();  }
  }

void Reverb::process(float* io, size_t frames) {
  if(!enabled_ || sampleRate_ == 0 || wet_ <= 0.f) return;

  for(size_t f=0; f<frames; ++f) {
    const float inL = io[f*2 + 0];
    const float inR = io[f*2 + 1];

    // Mono-ish comb input (sum of channels, scaled).
    const float input = (inL + inR) * kFixedGain;

    // Accumulate parallel combs.
    float outL = 0.f, outR = 0.f;
    for(int i=0; i<kNumCombs; ++i) {
      outL += combL_[i].tick(input);
      outR += combR_[i].tick(input);
      }
    // Series allpasses diffuse the echo density.
    for(int i=0; i<kNumAllpasses; ++i) {
      outL = apL_[i].tick(outL);
      outR = apR_[i].tick(outR);
      }

    // Mix dry + wet with stereo spread.
    io[f*2 + 0] = outL*wet1_ + outR*wet2_ + inL*dry_;
    io[f*2 + 1] = outR*wet1_ + outL*wet2_ + inR*dry_;
    }
  }
