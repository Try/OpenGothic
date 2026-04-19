#pragma once

// NT5 SVerb reverb — thin C++ wrapper around the original ksWaves / Microsoft
// SVerb engine (nt5/sverb.cpp). Exposed with the same public surface the
// mixer uses for our FreeVerb implementation so the two can be swapped
// by changing the type of Mixer::reverb_.
//
// Only compiled when OPENGOTHIC_WITH_NT5_DMSYNTH is enabled.
//
// SVerb internals:
//   • Coefficient table — one sCoefsStruct; we own it as an opaque byte blob
//     sized by GetCoefsSize() (the layout lives entirely inside sverb.h).
//   • State buffer    — BASE_REV_DELAY + 2*BASE_DSPS_DELAY + 2 longs
//     (~64 KB on 32-bit-long platforms). The process function uses the
//     first two longs as ring-buffer indices and the rest as delay lines.
//   • directGain / revGain — dry/wet mix, derived from dRevMix (dB).
//   • dRevTime            — RT60 in ms.
//   • dHighFreqRTRatio    — HF roll-off ratio of the tail (0..1, smaller = darker).
//
// Unlike our FreeVerb, SVerbStereoToStereoFloat consumes dry + produces dry+wet
// in one call (internally: Out = In*directGain + reverb_taps). So we call it
// directly on the mixer's interleaved stereo buffer, in place.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Dx8 {

class NT5Reverb final {
  public:
    NT5Reverb();
    ~NT5Reverb();

    NT5Reverb(const NT5Reverb&)            = delete;
    NT5Reverb& operator=(const NT5Reverb&) = delete;

    // (Re)allocate coefficient and state buffers for the given sample rate.
    // Call once before use; re-call if sample rate ever changes.
    void init(uint32_t sampleRate);

    // Enable/disable. When disabled, process() is a pass-through.
    void setEnabled(bool e) { enabled_ = e; }
    bool isEnabled() const  { return enabled_; }

    // Convenience FreeVerb-style knob: linear wet [0..1] → dRevMix dB
    // (0 → fully dry, 1 → approx -3 dB mix, matches I3DL2 "Music" loudness).
    void setWet(float v);

    // Full SVerb parameter set — bypass setWet() if you want exact control.
    //   inGainDb  — input gain, 0 dB leaves signal untouched (DirectMusic default: 0)
    //   revMixDb  — reverb mix in dB; 0 dB = 100 % wet, -96 dB = dry only
    //                (DirectMusic default: -10 dB ≈ -96 → ~0.316 wet ratio)
    //   revTimeMs — RT60 decay in ms (DirectMusic default: 1000)
    //   hfRatio   — high-freq RT60 to mid-freq RT60 ratio, 0.001..0.999
    //                (DirectMusic default: 0.001 → very dark tail)
    void setParams(float inGainDb, float revMixDb, float revTimeMs, float hfRatio);

    // FreeVerb-interface shims so mixer.cpp doesn't need #ifdefs per setter.
    // Room size [0..1] is mapped to RT60 0.5 s .. 3.0 s (I3DL2-ish range).
    void setRoomSize(float v);
    // Damping [0..1] maps to hfRatio 0.999..0.01 (0 = bright, 1 = dark).
    void setDamping (float v);
    // Width is not used by SVerb (stereo diffusion is fixed by topology).
    void setWidth   (float /*v*/) {}
    // Dry level: SVerb directGain is derived from revMix; we expose an extra
    // output-side scalar just in case someone dials the dry down explicitly.
    void setDry     (float v) { dryMul_ = v; }

    // Reset all delay lines to silence (call after pause/seek to avoid tail carry).
    void mute();

    // Process interleaved stereo in-place: samples == frames*2 floats.
    void process(float* io, size_t frames);

  private:
    void reconfigure();

    // We hold these as opaque byte blobs so sverb.h stays out of our public
    // header (its <windows.h> vestiges should not leak into other code).
    std::vector<uint8_t> coefs_;
    std::vector<uint8_t> states_;

    float    sampleRate_ = 0.f;
    float    inGainDb_   = 0.f;
    float    revMixDb_   = -10.f;
    float    revTimeMs_  = 1500.f;   // I3DL2 "Music" ≈ 1.49 s
    float    hfRatio_    = 0.55f;    // mild HF damping (not pitch-black like NT5 default)
    float    dryMul_     = 1.f;
    bool     enabled_    = true;
    bool     initialized_= false;
    bool     dirty_      = false;    // coef update needed before next process()
  };

} // namespace Dx8
