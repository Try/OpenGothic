#include "gamemusic.h"
#include "gothic.h"

#include <Tempest/Sound>
#include <Tempest/Log>

#include "game/definitions/musicdefinitions.h"
#include "dmusic/mixer.h"
#include "resources.h"
#include "dmusic.h"

using namespace Tempest;

static constexpr uint16_t SAMPLE_RATE = 44100;

struct GameMusic::MusicProvider : Tempest::SoundProducer {
  using Tempest::SoundProducer::SoundProducer;

  virtual void playTheme(const zenkit::IMusicTheme &theme, GameMusic::Tags tags) = 0;

  virtual void stopTheme() = 0;

  virtual void setEnabled(bool enable) = 0;

  virtual bool isEnabled() const = 0;

  virtual const std::optional<zenkit::IMusicTheme> getPlayingTheme() const = 0;
};

struct GameMusic::OpenGothicMusicProvider : GameMusic::MusicProvider {
  using GameMusic::MusicProvider::MusicProvider;

  void renderSound(int16_t *out, size_t n) override {
    if(!enable.load()) {
      std::memset(out, 0, n * sizeof(int32_t) * 2);
      return;
      }

    updateTheme();
    mix.mix(out, n);
    }

  void updateTheme() {
    zenkit::IMusicTheme theme;
    bool updateTheme = false;
    bool reloadTheme = false;
    Tags tags = Tags::Day;

    {
      std::lock_guard<std::mutex> guard(pendingSync);
      if(hasPending && pendingMusic && enable.load()) {
        hasPending  = false;
        updateTheme = true;
        reloadTheme = this->reloadTheme;
        theme       = *pendingMusic;
        tags        = pendingTags;
      }
    }

    if(!updateTheme)
      return;

    try {
      if(reloadTheme) {
        Dx8::PatternList p = Resources::loadDxMusic(theme.file);

        Dx8::Music m;
        m.addPattern(p);

        const int cur = currentTags & (Tags::Std | Tags::Fgt | Tags::Thr);
        const int next = tags & (Tags::Std | Tags::Fgt | Tags::Thr);

        Dx8::DMUS_EMBELLISHT_TYPES em = Dx8::DMUS_EMBELLISHT_END;
        if(next == Tags::Std) {
          if(cur != Tags::Std)
            em = Dx8::DMUS_EMBELLISHT_BREAK;
          }
        else if(next == Tags::Fgt) {
          if(cur == Tags::Thr)
            em = Dx8::DMUS_EMBELLISHT_FILL;
          }
        else if(next == Tags::Thr) {
          if(cur == Tags::Fgt)
            em = Dx8::DMUS_EMBELLISHT_NORMAL;
          }

        mix.setMusic(m, em);
        currentTags = tags;
        }
      mix.setMusicVolume(theme.vol);
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

  void playTheme(const zenkit::IMusicTheme &theme, GameMusic::Tags tags) override {
    std::lock_guard<std::mutex> guard(pendingSync);
    reloadTheme  = !pendingMusic || pendingMusic->file != theme.file;
    pendingMusic = theme;
    pendingTags  = tags;
    hasPending   = true;
    }

  void stopTheme() override {
    enable.store(false);
    mix.setMusic(Dx8::Music());
    pendingMusic.reset();
    }

  void setEnabled(bool b) override {
    if(enable == b)
      return;

    std::lock_guard<std::mutex> guard(pendingSync);
    if(b) {
      hasPending = true;
      reloadTheme = true;
      enable.store(true);
      } else {
      stopTheme();
      }
    }

  bool isEnabled() const override {
    return enable.load();
    }

  const std::optional<zenkit::IMusicTheme> getPlayingTheme() const override {
    return pendingMusic;
    }

private:
  Dx8::Mixer mix;

  std::mutex pendingSync;
  std::atomic_bool enable{true};
  bool hasPending = false;
  bool reloadTheme = false;
  std::optional<zenkit::IMusicTheme> pendingMusic;
  Tags pendingTags = Tags::Day;
  Tags currentTags = Tags::Day;
};

static std::pair<DmTiming, DmEmbellishmentType> getThemeEmbellishmentAndTiming(const zenkit::IMusicTheme &theme) {
  DmEmbellishmentType embellishment = DmEmbellishment_NONE;
  switch (theme.transtype) {
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
  switch (theme.transsubtype) {
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
    DmResult rv = DmPerformance_create(&performance, rate);
    if (rv != DmResult_SUCCESS) {
      Log::e("Unable to create DmPerformance object. Out of memory?");
    }
  }

  ~GothicKitMusicProvider() override {
    DmPerformance_release(performance);
  }

  void renderSound(int16_t *out, size_t n) override {
    if (!enabled.load()) {
      std::memset(out, 0, n * sizeof(int32_t) * 2);
      return;
    }

    DmPerformance_renderPcm(performance, out, n * 2, static_cast<DmRenderOptions>(DmRender_SHORT | DmRender_STEREO));
  }

  void playTheme(const zenkit::IMusicTheme &theme, GameMusic::Tags tags) override {
    (void) tags;

    if (!isEnabled()) return;
    if (playingTheme && theme.symbol_index() == playingTheme->symbol_index()) return;

    DmSegment *sgt = Resources::loadMusicSegment(theme.file.c_str());
    auto [timing, embellishment] = getThemeEmbellishmentAndTiming(theme);

    DmResult rv = DmPerformance_playTransition(performance, sgt, embellishment, timing);
    if (rv != DmResult_SUCCESS) {
      Log::e("Failed to play theme: ", theme.file);
      stopTheme();
    }

    DmPerformance_setVolume(performance, theme.vol);

    DmSegment_release(sgt);
    playingTheme = theme;
  }

  void stopTheme() override {
    DmPerformance_playTransition(performance, nullptr, DmEmbellishment_NONE, DmTiming_INSTANT);
  }

  void setEnabled(bool enable) override {
    enabled.store(enable);

    if (enable && playingTheme) {
      playTheme(*playingTheme, Tags::Std);
    } else {
      stopTheme();
    }
  }

  bool isEnabled() const override {
    return enabled.load() && performance != nullptr;
  }

  const std::optional<zenkit::IMusicTheme> getPlayingTheme() const override {
    return playingTheme;
  }

private:
  DmPerformance *performance = nullptr;
  std::atomic_bool enabled{true};
  std::optional<zenkit::IMusicTheme> playingTheme;
};

static constexpr int PROVIDER_OPENGOTHIC = 0;
// static constexpr int PROVIDER_GOTHICKIT = 1;

GameMusic *GameMusic::instance = nullptr;

GameMusic::GameMusic() {
  instance = this;

  std::unique_ptr<MusicProvider> p = std::make_unique<OpenGothicMusicProvider>(SAMPLE_RATE, 2);
  impl = p.get();
  provider = PROVIDER_OPENGOTHIC;

  sound = device.load(std::move(p));
  sound.play();
  sound.setVolume(0.5);

  Gothic::inst().onSettingsChanged.bind(this, &GameMusic::setupSettings);
  setupSettings();
  }

GameMusic::~GameMusic() {
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
  impl->playTheme(theme, tags);
  }

void GameMusic::stopMusic() {
  setEnabled(false);
  }

void GameMusic::setupSettings() {
  const int   musicEnabled  = Gothic::settingsGetI("SOUND",    "musicEnabled");
  const float musicVolume   = Gothic::settingsGetF("SOUND",    "musicVolume");
  const int   providerIndex = Gothic::settingsGetI("INTERNAL", "soundProviderIndex");

  if(providerIndex != provider) {
    Log::i("Switching music provider to ", providerIndex == PROVIDER_OPENGOTHIC ? "'OpenGothic'" : "'GothicKit'");

    auto playingTheme = impl->getPlayingTheme();
    impl->stopTheme();
    sound.pause();

    std::unique_ptr<MusicProvider> p;

    if(providerIndex == PROVIDER_OPENGOTHIC) {
      p = std::make_unique<OpenGothicMusicProvider>(SAMPLE_RATE, 2);
      } else {
      p = std::make_unique<GothicKitMusicProvider>(SAMPLE_RATE, 2);
      }

    impl = p.get();
    provider = providerIndex;

    if(playingTheme) {
      impl->playTheme(*playingTheme, Tags::Std);
      }

    sound = device.load(std::move(p));
    sound.play();
    }

  setEnabled(musicEnabled != 0);
  sound.setVolume(musicVolume);
  }
