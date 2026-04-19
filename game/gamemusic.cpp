#include "gamemusic.h"
#include "gothic.h"

#include <Tempest/Sound>
#include <Tempest/Log>

#include <algorithm>
#include <array>
#include <cmath>

#include "game/definitions/musicdefinitions.h"
#include "dmusic/mixer.h"
#include "dmusic/soundfont.h"
#include "resources.h"
#include "dmusic.h"

using namespace Tempest;

static constexpr uint16_t SAMPLE_RATE = 44100;

namespace {

float normalizeMusicReverbMix(float rawMix) {
  if(!std::isfinite(rawMix))
    return 0.0f;

  if(rawMix < 0.0f) {
    // Some scripts keep this in dB (roughly -96..0).
    return std::clamp(std::pow(10.0f, rawMix / 20.0f), 0.0f, 1.0f);
    }
  if(rawMix > 1.0f) {
    if(rawMix <= 100.0f)
      return std::clamp(rawMix / 100.0f, 0.0f, 1.0f);
    return std::clamp(rawMix / 10000.0f, 0.0f, 1.0f);
    }
  return std::clamp(rawMix, 0.0f, 1.0f);
  }

float normalizeMusicReverbTimeSeconds(float rawTime) {
  if(!std::isfinite(rawTime) || rawTime <= 0.0f)
    return 0.0f;

  // Theme data is usually in seconds, but some toolchains export milliseconds.
  const float seconds = rawTime > 30.0f ? (rawTime / 1000.0f) : rawTime;
  return std::clamp(seconds, 0.02f, 20.0f);
  }

struct ReverbCombLine final {
  std::vector<float> buffer;
  size_t             writeIndex  = 0;
  float              feedback    = 0.0f;
  float              damping     = 0.28f;
  float              filterState = 0.0f;

  void reset(size_t length) {
    buffer.assign(std::max<size_t>(1, length), 0.0f);
    writeIndex  = 0;
    filterState = 0.0f;
    }

  void clear() {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex  = 0;
    filterState = 0.0f;
    }

  float process(float sample) {
    if(buffer.empty())
      return sample;

    const float delayed = buffer[writeIndex];
    filterState         = delayed * (1.0f - damping) + filterState * damping;
    buffer[writeIndex]  = sample + filterState * feedback;
    writeIndex          = (writeIndex + 1) % buffer.size();
    return delayed;
    }
  };

struct ReverbAllPassLine final {
  std::vector<float> buffer;
  size_t             writeIndex = 0;
  float              feedback   = 0.50f;

  void reset(size_t length) {
    buffer.assign(std::max<size_t>(1, length), 0.0f);
    writeIndex = 0;
    }

  void clear() {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
    }

  float process(float sample) {
    if(buffer.empty())
      return sample;

    const float delayed = buffer[writeIndex];
    const float out     = -sample + delayed;
    buffer[writeIndex]  = sample + delayed * feedback;
    writeIndex          = (writeIndex + 1) % buffer.size();
    return out;
    }
  };

class SimpleStereoReverb final {
  public:
    void configure(int sampleRate, int channels) {
      const int newSampleRate  = sampleRate > 0 ? sampleRate : int(SAMPLE_RATE);
      const int newNumChannels = channels == 1 ? 1 : 2;
      if(this->sampleRate == newSampleRate && numChannels == newNumChannels && hasBuffers)
        return;

      this->sampleRate = newSampleRate;
      numChannels      = newNumChannels;
      allocateLines();
      updateFeedbackCoefficients();
      if(!enabled)
        clearState();
      }

    void setParameters(float rawMix, float rawTime) {
      const float normalizedMix  = normalizeMusicReverbMix(rawMix);
      const float normalizedTime = normalizeMusicReverbTimeSeconds(rawTime);
      mixAmount         = std::clamp(normalizedMix, 0.0f, 0.85f);
      reverbTimeSeconds = normalizedTime > 0.0f ? std::clamp(normalizedTime, 0.08f, 6.0f) : 0.0f;
      enabled           = mixAmount > 0.002f && reverbTimeSeconds > 0.05f;
      updateFeedbackCoefficients();
      if(!enabled)
        clearState();
      }

    void processPcm16InPlace(int16_t* pcm, size_t numFrames) {
      if(!enabled || pcm == nullptr || numFrames == 0 || sampleRate <= 0)
        return;
      if(numChannels != 1 && numChannels != 2)
        return;

      const float dryMix    = 1.0f - mixAmount * 0.55f;
      const float wetMix    = mixAmount * 0.65f;
      const float crossFeed = numChannels == 2 ? 0.03f : 0.0f;

      for(size_t i = 0; i < numFrames; ++i) {
        const size_t sampleIndex = i * size_t(numChannels);

        const float dryL = float(pcm[sampleIndex]) / 32768.0f;
        const float dryR = numChannels == 2 ? float(pcm[sampleIndex + 1]) / 32768.0f : dryL;

        float wetL = processChannel(dryL + crossFeed * dryR, combLeft, allPassLeft);
        float wetR = processChannel(dryR + crossFeed * dryL, combRight, allPassRight);
        if(numChannels == 1)
          wetL = wetR = 0.5f * (wetL + wetR);

        const float outL = dryL * dryMix + wetL * wetMix;
        const float outR = dryR * dryMix + wetR * wetMix;
        pcm[sampleIndex] = toPcm16(outL);
        if(numChannels == 2)
          pcm[sampleIndex + 1] = toPcm16(outR);
        }
      }

  private:
    static constexpr size_t NumCombLines    = 8;
    static constexpr size_t NumAllPassLines = 2;

    int   sampleRate        = int(SAMPLE_RATE);
    int   numChannels       = 2;
    float mixAmount         = 0.0f;
    float reverbTimeSeconds = 0.0f;
    bool  enabled           = false;
    bool  hasBuffers        = false;

    std::array<ReverbCombLine, NumCombLines>       combLeft;
    std::array<ReverbCombLine, NumCombLines>       combRight;
    std::array<ReverbAllPassLine, NumAllPassLines> allPassLeft;
    std::array<ReverbAllPassLine, NumAllPassLines> allPassRight;

    void allocateLines() {
      static constexpr std::array<int, NumCombLines> combBase   = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
      static constexpr std::array<int, NumCombLines> combSpread = {  23,   23,   23,   23,   23,   23,   23,   23};
      static constexpr std::array<int, NumAllPassLines> allBase   = {556, 441};
      static constexpr std::array<int, NumAllPassLines> allSpread = {19, 13};

      const float rateScale = float(sampleRate) / 44100.0f;
      for(size_t i = 0; i < combLeft.size(); ++i) {
        const int leftLen  = std::max(1, int(float(combBase[i]) * rateScale));
        const int rightLen = std::max(1, int(float(combBase[i] + combSpread[i]) * rateScale));
        combLeft[i].reset(size_t(leftLen));
        combRight[i].reset(size_t(rightLen));
        }
      for(size_t i = 0; i < allPassLeft.size(); ++i) {
        const int leftLen  = std::max(1, int(float(allBase[i]) * rateScale));
        const int rightLen = std::max(1, int(float(allBase[i] + allSpread[i]) * rateScale));
        allPassLeft[i].reset(size_t(leftLen));
        allPassRight[i].reset(size_t(rightLen));
        }
      hasBuffers = true;
      }

    void clearState() {
      for(auto& line : combLeft)
        line.clear();
      for(auto& line : combRight)
        line.clear();
      for(auto& line : allPassLeft)
        line.clear();
      for(auto& line : allPassRight)
        line.clear();
      }

    void updateFeedbackCoefficients() {
      if(!hasBuffers)
        return;

      const float t60     = std::max(0.08f, reverbTimeSeconds);
      const float damping = std::clamp(0.28f + mixAmount * 0.18f, 0.18f, 0.62f);
      for(auto& line : combLeft) {
        const float delaySec = float(std::max<size_t>(1, line.buffer.size())) / float(sampleRate);
        line.feedback        = std::clamp(std::pow(10.0f, -3.0f * delaySec / t60), 0.02f, 0.95f);
        line.damping         = damping;
        }
      for(auto& line : combRight) {
        const float delaySec = float(std::max<size_t>(1, line.buffer.size())) / float(sampleRate);
        line.feedback        = std::clamp(std::pow(10.0f, -3.0f * delaySec / t60), 0.02f, 0.95f);
        line.damping         = damping;
        }
      }

    static float processChannel(float input,
                                std::array<ReverbCombLine, NumCombLines>& comb,
                                std::array<ReverbAllPassLine, NumAllPassLines>& allPass) {
      float accum = 0.0f;
      for(auto& line : comb)
        accum += line.process(input);
      float value = accum / float(comb.size());
      for(auto& line : allPass)
        value = line.process(value);
      return value;
      }

    static int16_t toPcm16(float sample) {
      if(sample < -1.00004566f)
        return int16_t(-32768);
      if(sample > 1.00001514f)
        return int16_t(32767);
      return int16_t(sample * 32767.5f);
      }
  };

}

struct GameMusic::MusicProvider : Tempest::SoundProducer {
  using Tempest::SoundProducer::SoundProducer;

  public:
    virtual void stopTheme() {
      std::lock_guard<std::recursive_mutex> guard(pendingSync);
      pendingMusic.reset();
      }

    virtual void getChannelStates(std::vector<GameMusic::ChannelInfo>&) const {}
    virtual void setChannelSolo (uint32_t /*pChannel*/) {}
    virtual void setChannelMuted(uint32_t /*pChannel*/, bool /*muted*/) {}
    virtual void clearChannelIsolation() {}

    void setEnabled(bool b) {
      if(enable == b)
        return;

      std::lock_guard<std::recursive_mutex> guard(pendingSync);
      if(b) {
        hasPending = true;
        reloadTheme = true;
        enable.store(true);
        } else {
        enable.store(false);
        stopTheme();
        }
      }

    bool isEnabled() const {
      return enable.load();
      }

    void playTheme(const zenkit::IMusicTheme &theme, GameMusic::Tags tags) {
      std::lock_guard<std::recursive_mutex> guard(pendingSync);
      reloadTheme  = !pendingMusic || pendingMusic->file != theme.file;
      pendingMusic = theme;
      pendingTags  = tags;
      hasPending   = true;
      }

  protected:
    void setMusicReverb(float reverbMix, float reverbTime) {
      reverb.configure(int(SAMPLE_RATE), 2);
      reverb.setParameters(reverbMix, reverbTime);
      }

    void processMusicReverb(int16_t* out, size_t frames) {
      reverb.processPcm16InPlace(out, frames);
      }

    bool updateTheme(zenkit::IMusicTheme& theme, Tags& tags) {
      bool reloadTheme = false;

      std::lock_guard<std::recursive_mutex> guard(pendingSync);
      if(hasPending && pendingMusic && enable.load()) {
        hasPending  = false;
        reloadTheme = this->reloadTheme;
        theme       = this->pendingMusic.value();
        tags        = this->pendingTags;
        }

      this->reloadTheme = false;
      return reloadTheme;
      }

    static zenkit::MusicTransitionEffect computeTransitionEffect(Tags nextTags, Tags currTags, zenkit::MusicTransitionEffect transtype) {
      const int cur  = currTags & (Tags::Std | Tags::Fgt | Tags::Thr);
      const int next = nextTags & (Tags::Std | Tags::Fgt | Tags::Thr);

      //zenkit::MusicTransitionEffect embellishment = transtype;
      zenkit::MusicTransitionEffect embellishment = zenkit::MusicTransitionEffect::NONE;
      if(next == Tags::Std) {
        if(cur != Tags::Std)
          embellishment = zenkit::MusicTransitionEffect::BREAK;
        }
      else if(next == Tags::Fgt) {
        if(cur == Tags::Thr)
          embellishment = zenkit::MusicTransitionEffect::FILL;
        }
      else if(next == Tags::Thr) {
        if(cur == Tags::Fgt)
          embellishment = zenkit::MusicTransitionEffect::NONE;
        }

      return embellishment;
      }

  private:
    std::atomic_bool     enable{true};
    SimpleStereoReverb   reverb;

    std::recursive_mutex pendingSync;
    bool                 hasPending  = false;
    bool                 reloadTheme = false;
    Tags                 pendingTags = Tags::Day;
    std::optional<zenkit::IMusicTheme> pendingMusic;

  protected:
    Tags                 currentTags = Tags::Day;
  };

struct GameMusic::OpenGothicMusicProvider : GameMusic::MusicProvider {
  using GameMusic::MusicProvider::MusicProvider;

  void renderSound(int16_t *out, size_t n) override {
    if(!isEnabled()) {
      std::memset(out, 0, n * sizeof(int16_t) * 2);
      return;
      }
    updateTheme();
    mix.mix(out, n);
    processMusicReverb(out, n);
    }

  void updateTheme() {
    zenkit::IMusicTheme theme;
    Tags                tags;
    if(!GameMusic::MusicProvider::updateTheme(theme, tags))
      return;

    if(theme.file.empty()) {
      stopTheme();
      return;
      }

    try {
      if(/*reloadTheme*/true) {
        Dx8::PatternList p = Resources::loadDxMusic(theme.file);

        Dx8::Music m;
        m.addPattern(p);

        auto effect = computeTransitionEffect(tags, currentTags, theme.transtype);
        Dx8::DMUS_EMBELLISHT_TYPES em = computeEmbellishment(effect);
        mix.setMusic(m, em);
        currentTags = tags;
        }
      mix.setMusicVolume(theme.vol);
      setMusicReverb(theme.reverbmix, theme.reverbtime);
      }
    catch (std::runtime_error &) {
      Log::e("unable to load sound: \"", theme.file, "\"");
      stopTheme();
      }
    catch (std::bad_alloc &) {
      Log::e("out of memory for sound: \"", theme.file, "\"");
      stopTheme();
      }
    }

  void stopTheme() override {
    GameMusic::MusicProvider::stopTheme();
    mix.setMusic(Dx8::Music());
    setMusicReverb(0.0f, 0.0f);
    }

  static Dx8::DMUS_EMBELLISHT_TYPES computeEmbellishment(zenkit::MusicTransitionEffect ef) {
    switch (ef) {
      case zenkit::MusicTransitionEffect::UNKNOWN:
      case zenkit::MusicTransitionEffect::NONE:
        return Dx8::DMUS_EMBELLISHT_NORMAL;
      case zenkit::MusicTransitionEffect::GROOVE:
        return Dx8::DMUS_EMBELLISHT_NORMAL;
      case zenkit::MusicTransitionEffect::FILL:
        return Dx8::DMUS_EMBELLISHT_FILL;
      case zenkit::MusicTransitionEffect::BREAK:
        return Dx8::DMUS_EMBELLISHT_BREAK;
      case zenkit::MusicTransitionEffect::INTRO:
        return Dx8::DMUS_EMBELLISHT_INTRO;
      case zenkit::MusicTransitionEffect::END:
      case zenkit::MusicTransitionEffect::END_AND_INTO:
        return Dx8::DMUS_EMBELLISHT_END;
      }
    return Dx8::DMUS_EMBELLISHT_NORMAL;
    }

  void getChannelStates(std::vector<GameMusic::ChannelInfo>& out) const override {
    std::vector<Dx8::Mixer::ChannelState> raw;
    mix.getChannelStates(raw);
    out.clear();
    out.reserve(raw.size());
    for(size_t i = 0; i < raw.size(); ++i) {
      GameMusic::ChannelInfo ci;
      ci.index        = i;
      ci.pChannel     = raw[i].pChannel;
      ci.dwPatch      = raw[i].dwPatch;
      ci.name         = raw[i].instrumentName;
      ci.volume       = raw[i].volume;
      ci.pan          = raw[i].pan;
      ci.activeVoices = raw[i].activeVoices;
      ci.isMuted      = raw[i].isMuted;
      ci.isSolo       = raw[i].isSolo;
      out.push_back(std::move(ci));
      }
    }

  void setChannelSolo(uint32_t pChannel) override {
    mix.setChannelSolo(pChannel);
    }

  void setChannelMuted(uint32_t pChannel, bool muted) override {
    mix.setChannelMuted(pChannel, muted);
    }

  void clearChannelIsolation() override {
    mix.clearChannelIsolation();
    }

  private:
    Dx8::Mixer mix;
  };

static std::pair<DmTiming, DmEmbellishmentType> computeEmbellishmentAndTiming(const zenkit::MusicTransitionEffect effect, const zenkit::MusicTransitionType type) {
  DmEmbellishmentType embellishment = DmEmbellishment_NONE;
  switch (effect) {
    case zenkit::MusicTransitionEffect::UNKNOWN:
    case zenkit::MusicTransitionEffect::NONE:
      embellishment = DmEmbellishment_NONE;
      break;
    case zenkit::MusicTransitionEffect::GROOVE:
      embellishment = DmEmbellishment_GROOVE;
      break;
    case zenkit::MusicTransitionEffect::FILL:
      embellishment = DmEmbellishment_FILL;
      break;
    case zenkit::MusicTransitionEffect::BREAK:
      embellishment = DmEmbellishment_BREAK;
      break;
    case zenkit::MusicTransitionEffect::INTRO:
      embellishment = DmEmbellishment_INTRO;
      break;
    case zenkit::MusicTransitionEffect::END:
      embellishment = DmEmbellishment_END;
      break;
    case zenkit::MusicTransitionEffect::END_AND_INTO:
      embellishment = DmEmbellishment_END_AND_INTRO;
      break;
    }

  DmTiming timing = DmTiming_MEASURE;
  switch (type) {
    case zenkit::MusicTransitionType::UNKNOWN:
    case zenkit::MusicTransitionType::MEASURE:
      timing = DmTiming_MEASURE;
      break;
    case zenkit::MusicTransitionType::IMMEDIATE:
      timing = DmTiming_INSTANT;
      break;
    case zenkit::MusicTransitionType::BEAT:
      timing = DmTiming_BEAT;
      break;
    }

  return std::make_pair(timing, embellishment);
  }

struct GameMusic::GothicKitMusicProvider : GameMusic::MusicProvider {
  GothicKitMusicProvider(uint16_t rate, uint16_t channels) : GameMusic::MusicProvider(rate, channels) {
    /*
    Dm_setRandomNumberGenerator([](void*) -> uint32_t {
      return 0;
      }, nullptr);
    */

    DmResult rv = DmPerformance_create(&performance, rate);
    if(rv != DmResult_SUCCESS) {
      Log::e("Unable to create DmPerformance object. Out of memory?");
      }
    }

  ~GothicKitMusicProvider() override {
    DmPerformance_release(performance);
    }

  void renderSound(int16_t *out, size_t n) override {
    if(!isEnabled()) {
      std::memset(out, 0, n * sizeof(int16_t) * 2);
      return;
      }
    updateTheme();
    DmPerformance_renderPcm(performance, out, n * 2, DmRenderOptions(DmRender_SHORT | DmRender_STEREO));
    processMusicReverb(out, n);
    }

  void updateTheme() {
    zenkit::IMusicTheme theme;
    Tags                tags;
    if(!GameMusic::MusicProvider::updateTheme(theme, tags))
      return;

    if(theme.file.empty()) {
      stopTheme();
      return;
      }

    auto effect = computeTransitionEffect(tags, currentTags, theme.transtype);
    auto [timing, embellishment] = computeEmbellishmentAndTiming(effect, theme.transsubtype);

    DmSegment* sgt = Resources::loadMusicSegment(theme.file.c_str());
    DmResult rv = DmPerformance_playTransition(performance, sgt, embellishment, timing);
    if(rv != DmResult_SUCCESS) {
      Log::e("Failed to play theme: ", theme.file);
      stopTheme();
      }

    DmPerformance_setVolume(performance, theme.vol);
    setMusicReverb(theme.reverbmix, theme.reverbtime);
    DmSegment_release(sgt);
    currentTags = tags;
    }

  void stopTheme() override {
    GameMusic::MusicProvider::stopTheme();
    DmPerformance_playTransition(performance, nullptr, DmEmbellishment_NONE, DmTiming_INSTANT);
    setMusicReverb(0.0f, 0.0f);
    }

  DmEmbellishmentType computeEmbellishment(Tags nextTags, Tags currTags, zenkit::MusicTransitionEffect transtype) const {
    const int cur  = currTags & (Tags::Std | Tags::Fgt | Tags::Thr);
    const int next = nextTags & (Tags::Std | Tags::Fgt | Tags::Thr);

    DmEmbellishmentType embellishment = DmEmbellishment_NONE;
    switch (transtype) {
      case zenkit::MusicTransitionEffect::UNKNOWN:
      case zenkit::MusicTransitionEffect::NONE:
        embellishment = DmEmbellishment_NONE;
        break;
      case zenkit::MusicTransitionEffect::GROOVE:
        embellishment = DmEmbellishment_GROOVE;
        break;
      case zenkit::MusicTransitionEffect::FILL:
        embellishment = DmEmbellishment_FILL;
        break;
      case zenkit::MusicTransitionEffect::BREAK:
        embellishment = DmEmbellishment_BREAK;
        break;
      case zenkit::MusicTransitionEffect::INTRO:
        embellishment = DmEmbellishment_INTRO;
        break;
      case zenkit::MusicTransitionEffect::END:
        embellishment = DmEmbellishment_END;
        break;
      case zenkit::MusicTransitionEffect::END_AND_INTO:
        embellishment = DmEmbellishment_END_AND_INTRO;
        break;
      }

    if(next == Tags::Std) {
      if(cur != Tags::Std)
        embellishment = DmEmbellishmentType::DmEmbellishment_BREAK;
      }
    else if(next == Tags::Fgt) {
      if(cur == Tags::Thr)
        embellishment = DmEmbellishmentType::DmEmbellishment_FILL;
      }
    else if(next == Tags::Thr) {
      if(cur == Tags::Fgt)
        embellishment = DmEmbellishmentType::DmEmbellishment_NONE;
      }

    return embellishment;
    }

  private:
    DmPerformance *performance = nullptr;
  };

// ─────────────────────────────────────────────────────────────────────────────
// DXMusicProvider — fully independent provider using authentic NT5 dmsynth.
// Same pattern sequencer as OpenGothicMusicProvider (Dx8::Mixer) but forces
// the DxSynth backend: no Hydra SF2 conversion, no TinySoundFont.
// This is the default provider.
// ─────────────────────────────────────────────────────────────────────────────
struct GameMusic::DXMusicProvider : GameMusic::MusicProvider {
  DXMusicProvider(uint16_t rate, uint16_t channels)
    : GameMusic::MusicProvider(rate, channels) {
    // Force the DxSynth backend for this provider's lifetime.
    // Any SoundFont::Data created while this provider is active will use
    // pure DLS synthesis (no Hydra/SF2/tsf).
    prevBackend = Dx8::SoundFont::activeBackend();
    Dx8::SoundFont::setBackend(Dx8::SoundFont::Backend::DxSynth);
    Log::i("DXMusicProvider: using authentic NT5 dmsynth backend");
    }

  ~DXMusicProvider() override {
    Dx8::SoundFont::setBackend(prevBackend);
    }

  void renderSound(int16_t *out, size_t n) override {
    if(!isEnabled()) {
      std::memset(out, 0, n * sizeof(int16_t) * 2);
      return;
      }
    updateTheme();
    mix.mix(out, n);
    processMusicReverb(out, n);
    }

  void updateTheme() {
    zenkit::IMusicTheme theme;
    Tags                tags;
    if(!GameMusic::MusicProvider::updateTheme(theme, tags))
      return;

    if(theme.file.empty()) {
      stopTheme();
      return;
      }

    try {
      Dx8::PatternList p = Resources::loadDxMusic(theme.file);
      Dx8::Music m;
      m.addPattern(p);

      auto effect = computeTransitionEffect(tags, currentTags, theme.transtype);
      Dx8::DMUS_EMBELLISHT_TYPES em = computeEmbellishment(effect);
      mix.setMusic(m, em);
      currentTags = tags;

      mix.setMusicVolume(theme.vol);
      setMusicReverb(theme.reverbmix, theme.reverbtime);
      }
    catch(const std::exception& e) {
      Log::e("DXMusicProvider: unable to load \"", theme.file, "\": ", e.what());
      stopTheme();
      }
    }

  void stopTheme() override {
    GameMusic::MusicProvider::stopTheme();
    mix.setMusic(Dx8::Music());
    setMusicReverb(0.0f, 0.0f);
    }

  static Dx8::DMUS_EMBELLISHT_TYPES computeEmbellishment(zenkit::MusicTransitionEffect ef) {
    switch(ef) {
      case zenkit::MusicTransitionEffect::FILL:  return Dx8::DMUS_EMBELLISHT_FILL;
      case zenkit::MusicTransitionEffect::BREAK: return Dx8::DMUS_EMBELLISHT_BREAK;
      case zenkit::MusicTransitionEffect::INTRO: return Dx8::DMUS_EMBELLISHT_INTRO;
      case zenkit::MusicTransitionEffect::END:
      case zenkit::MusicTransitionEffect::END_AND_INTO: return Dx8::DMUS_EMBELLISHT_END;
      default: return Dx8::DMUS_EMBELLISHT_NORMAL;
      }
    }

  private:
    Dx8::SoundFont::Backend prevBackend = Dx8::SoundFont::Backend::DxSynth;
    Dx8::Mixer              mix;
  };

// ─────────────────────────────────────────────────────────────────────────────
static constexpr int PROVIDER_UNINITIALIZED = -1;
static constexpr int PROVIDER_DXMUSIC    = 0; // default — authentic NT5 dmsynth
static constexpr int PROVIDER_OPENGOTHIC = 1; // TinySoundFont
// static constexpr int PROVIDER_GOTHICKIT = 2;

GameMusic *GameMusic::instance = nullptr;

GameMusic::GameMusic() {
  instance = this;
  provider = PROVIDER_UNINITIALIZED;

  Gothic::inst().onSettingsChanged.bind(this, &GameMusic::setupSettings);
  setupSettings();
  }

GameMusic::~GameMusic() {
  sound = SoundEffect();
  instance = nullptr;
  Gothic::inst().onSettingsChanged.ubind(this, &GameMusic::setupSettings);
  }

GameMusic &GameMusic::inst() {
  return *instance;
  }

GameMusic::Tags GameMusic::mkTags(GameMusic::Tags daytime, GameMusic::Tags mode) {
  return Tags(daytime | mode);
  }

void GameMusic::setEnabled(bool e) {
  impl->setEnabled(e);
  }

bool GameMusic::isEnabled() const {
  return impl->isEnabled();
  }

void GameMusic::setMusic(GameMusic::Music m) {
  const char *clsTheme = "";
  switch (m) {
    case GameMusic::SysLoading:
      clsTheme = "SYS_Loading";
      break;
    }
  if(auto theme = Gothic::musicDef()[clsTheme])
    setMusic(*theme, GameMusic::mkTags(GameMusic::Std, GameMusic::Day));
  }

void GameMusic::setMusic(const zenkit::IMusicTheme &theme, Tags tags) {
  currentMusic.theme = theme;
  currentMusic.tags  = tags;
  impl->playTheme(currentMusic.theme, currentMusic.tags);
  }

void GameMusic::stopMusic() {
  setEnabled(false);
  }

void GameMusic::getChannelStates(std::vector<ChannelInfo>& out) const {
  if(impl) impl->getChannelStates(out);
  }

void GameMusic::setChannelSolo(size_t index) {
  std::vector<ChannelInfo> ch;
  getChannelStates(ch);
  if(index < ch.size() && impl)
    impl->setChannelSolo(ch[index].pChannel);
  }

void GameMusic::setChannelMuted(size_t index, bool muted) {
  std::vector<ChannelInfo> ch;
  getChannelStates(ch);
  if(index < ch.size() && impl)
    impl->setChannelMuted(ch[index].pChannel, muted);
  }

void GameMusic::clearChannelIsolation() {
  if(impl) impl->clearChannelIsolation();
  }

void GameMusic::setupSettings() {
  const int   musicEnabled  = Gothic::settingsGetI("SOUND",    "musicEnabled");
  const float musicVolume   = Gothic::settingsGetF("SOUND",    "musicVolume");
  const int   providerIndex = Gothic::settingsGetI("INTERNAL", "soundProviderIndex");

  if(providerIndex != provider) {
    sound = SoundEffect();

    std::unique_ptr<MusicProvider> p;
    if(providerIndex == PROVIDER_DXMUSIC) {
      Log::i("Music provider: DXMusicProvider (NT5 dmsynth)");
      p = std::make_unique<DXMusicProvider>(SAMPLE_RATE, 2);
      } else if(providerIndex == PROVIDER_OPENGOTHIC) {
      Log::i("Music provider: OpenGothicMusicProvider (TinySoundFont)");
      p = std::make_unique<OpenGothicMusicProvider>(SAMPLE_RATE, 2);
      } else {
      Log::i("Music provider: GothicKitMusicProvider");
      p = std::make_unique<GothicKitMusicProvider>(SAMPLE_RATE, 2);
      }

    provider = providerIndex;
    impl = p.get();
    impl->playTheme(currentMusic.theme, currentMusic.tags);

    sound = device.load(std::move(p));
    sound.play();
    }

  setEnabled(musicEnabled != 0);
  sound.setVolume(musicVolume);
  }
