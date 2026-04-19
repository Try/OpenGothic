#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>

namespace Dx8 {

// FreeVerb-style Schroeder reverb (8 LP combs + 4 allpass per channel, stereo).
//
// Designed as a drop-in post-processing stage for the DirectMusic mixer.
// Topology matches Jezar Wakefield's public-domain FreeVerb with tunings
// scaled to the current sample rate. Defaults are tuned to approximate the
// I3DL2 "Music" / "Generic" preset used by DirectMusic Producer's
// "Stereo + Reverb" mode (the mode Gothic's bank was authored in).
//
// Use from the audio thread only: process() mutates internal buffer state.
class Reverb final {
  public:
    Reverb();

    // (Re)allocate internal delay lines for the given sample rate.
    // Call once before use; re-call if sample rate ever changes.
    void init(uint32_t sampleRate);

    // Room size in [0..1]. Larger == longer decay. Default 0.5.
    void setRoomSize(float v);
    // High-frequency damping in [0..1]. Larger == darker tail. Default 0.5.
    void setDamping(float v);
    // Stereo width in [0..1]. Default 1.0 (full stereo).
    void setWidth(float v);
    // Wet level in [0..1]. 0 disables reverb entirely. Default 0.33.
    void setWet(float v);
    // Dry level in [0..1]. Default 1.0 (dry always at unity).
    void setDry(float v);
    // Enable/disable. When disabled, process() is a cheap no-op.
    void setEnabled(bool e) { enabled_ = e; }
    bool isEnabled() const  { return enabled_; }

    // Reset all delay lines to silence (call after pause/seek to avoid tail carry).
    void mute();

    // Process interleaved stereo in-place: samples == frames*2 floats.
    void process(float* io, size_t frames);

  private:
    struct Comb {
      std::vector<float> buf;
      size_t             idx       = 0;
      float              feedback  = 0.f;
      float              damp1     = 0.f;
      float              damp2     = 1.f;
      float              filterst  = 0.f;
      void   resize(size_t n) { buf.assign(n, 0.f); idx = 0; filterst = 0.f; }
      float  tick(float in);
      void   mute() { std::fill(buf.begin(), buf.end(), 0.f); filterst = 0.f; }
      };

    struct Allpass {
      std::vector<float> buf;
      size_t             idx      = 0;
      float              feedback = 0.5f;
      void   resize(size_t n) { buf.assign(n, 0.f); idx = 0; }
      float  tick(float in);
      void   mute() { std::fill(buf.begin(), buf.end(), 0.f); }
      };

    void applyParams();

    // Gain applied to the pre-mixed input to prevent comb buildup (FreeVerb constant).
    static constexpr float kFixedGain = 0.015f;
    // Feedback scale / offset for roomSize (FreeVerb constants).
    static constexpr float kScaleRoom  = 0.28f;
    static constexpr float kOffsetRoom = 0.7f;
    // Damping scale (FreeVerb).
    static constexpr float kScaleDamp  = 0.4f;

    static constexpr int   kNumCombs    = 8;
    static constexpr int   kNumAllpasses = 4;

    Comb    combL_[kNumCombs];
    Comb    combR_[kNumCombs];
    Allpass apL_[kNumAllpasses];
    Allpass apR_[kNumAllpasses];

    float   roomSize_ = 0.5f;
    float   damping_  = 0.5f;
    float   width_    = 1.0f;
    float   wet_      = 0.33f;
    float   dry_      = 1.0f;

    // Derived:
    float   wet1_ = 0.f;  // L→L, R→R
    float   wet2_ = 0.f;  // R→L, L→R (width cross-mix)

    bool    enabled_ = true;
    uint32_t sampleRate_ = 0;
  };

}
