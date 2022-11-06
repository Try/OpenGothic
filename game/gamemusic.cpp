#include "gamemusic.h"
#include "gothic.h"

#include <Tempest/Sound>
#include <Tempest/Log>

#include "game/definitions/musicdefinitions.h"
#include "dmusic/mixer.h"
#include "resources.h"

using namespace Tempest;

struct GameMusic::MusicProducer : Tempest::SoundProducer {
  MusicProducer():SoundProducer(44100,2){
    }

  void renderSound(int16_t* out,size_t n) override {
    updateTheme();
    mix.mix(out,n);
    }

  void updateTheme() {
    phoenix::c_music_theme       theme;
    bool                                   updateTheme=false;
    bool                                   reloadTheme=false;
    Tags                                   tags=Tags::Day;

    {
      std::lock_guard<std::mutex> guard(pendingSync);
      if(hasPending && enable.load()) {
        hasPending  = false;
        updateTheme = true;
        reloadTheme = this->reloadTheme;
        theme       = pendingMusic;
        tags        = pendingTags;
        }
    }

    if(!updateTheme)
      return;
    updateTheme = false;

    try {
      if(reloadTheme) {
        Dx8::PatternList p = Resources::loadDxMusic(theme.file);

        Dx8::Music m;
        m.addPattern(p);

        const int cur  = currentTags&(Tags::Std|Tags::Fgt|Tags::Thr);
        const int next = tags&(Tags::Std|Tags::Fgt|Tags::Thr);

        Dx8::DMUS_EMBELLISHT_TYPES em = Dx8::DMUS_EMBELLISHT_END;
        if(next==Tags::Std) {
          if(cur!=Tags::Std)
            em = Dx8::DMUS_EMBELLISHT_BREAK;
          } else
        if(next==Tags::Fgt){
          if(cur==Tags::Thr)
            em = Dx8::DMUS_EMBELLISHT_FILL;
          } else
        if(next==Tags::Thr){
          if(cur==Tags::Fgt)
            em = Dx8::DMUS_EMBELLISHT_NORMAL;
          }

        mix.setMusic(m,em);
        currentTags=tags;
        }
      mix.setMusicVolume(theme.vol);
      }
    catch(std::runtime_error&) {
      Log::e("unable to load sound: \"",theme.file,"\"");
      }
    }

  bool setMusic(const phoenix::c_music_theme &theme, Tags tags){
    std::lock_guard<std::mutex> guard(pendingSync);
    reloadTheme  = pendingMusic.file!=theme.file;
    pendingMusic = theme;
    pendingTags  = tags;
    hasPending   = true;
    return true;
    }

  void restartMusic(){
    std::lock_guard<std::mutex> guard(pendingSync);
    hasPending  = true;
    reloadTheme = true;
    enable.store(true);
    }

  void stopMusic() {
    enable.store(false);
    std::lock_guard<std::mutex> guard(pendingSync);
    mix.setMusic(Dx8::Music());
    }

  void setVolume(float v) {
    mix.setVolume(v);
    }

  bool isEnabled() const {
    return enable.load();
    }

  Dx8::Mixer                             mix;

  std::mutex                             pendingSync;
  std::atomic_bool                       enable{true};
  bool                                   hasPending=false;
  bool                                   reloadTheme=false;
  phoenix::c_music_theme                 pendingMusic;
  Tags                                   pendingTags=Tags::Day;
  Tags                                   currentTags=Tags::Day;
  };

struct GameMusic::Impl final {
  Impl() {
    std::unique_ptr<MusicProducer> mix(new MusicProducer());
    dxMixer = mix.get();
    sound   = device.load(std::move(mix));
    dxMixer->setVolume(0.5f);
    }

  void setMusic(const phoenix::c_music_theme &theme, Tags tags) {
    dxMixer->setMusic(theme,tags);
    }

  void setVolume(float v) {
    dxMixer->setVolume(v);
    }

  void setEnabled(bool e) {
    if(isEnabled()==e)
      return;
    if(e) {
      sound.play();
      dxMixer->restartMusic();
      } else {
      dxMixer->stopMusic();
      }
    }

  bool isEnabled() const {
    return dxMixer->isEnabled();
    }

  Tempest::SoundDevice device;
  Tempest::SoundEffect sound;

  MusicProducer*       dxMixer=nullptr;
  };

GameMusic* GameMusic::instance = nullptr;

GameMusic::GameMusic() {
  instance = this;
  impl.reset(new Impl());
  Gothic::inst().onSettingsChanged.bind(this,&GameMusic::setupSettings);
  setupSettings();
  }

GameMusic::~GameMusic() {
  instance = nullptr;
  Gothic::inst().onSettingsChanged.ubind(this,&GameMusic::setupSettings);
  }

GameMusic& GameMusic::inst() {
  return *instance;
  }

GameMusic::Tags GameMusic::mkTags(GameMusic::Tags daytime, GameMusic::Tags mode) {
  return Tags(daytime|mode);
  }

void GameMusic::setEnabled(bool e) {
  impl->setEnabled(e);
  }

bool GameMusic::isEnabled() const {
  return impl->isEnabled();
  }

void GameMusic::setMusic(GameMusic::Music m) {
  const char* clsTheme="";
  switch(m) {
    case GameMusic::SysLoading:
      clsTheme = "SYS_Loading";
      break;
    }
  if(auto theme = Gothic::musicDef()[clsTheme])
    setMusic(*theme,GameMusic::mkTags(GameMusic::Std,GameMusic::Day));
  }

void GameMusic::setMusic(const phoenix::c_music_theme &theme, Tags tags) {
  impl->setMusic(theme,tags);
  }

void GameMusic::stopMusic() {
  setEnabled(false);
  }

void GameMusic::setupSettings() {
  const int   musicEnabled = Gothic::settingsGetI("SOUND","musicEnabled");
  const float musicVolume  = Gothic::settingsGetF("SOUND","musicVolume");

  setEnabled(musicEnabled!=0);
  impl->setVolume(musicVolume);
  }
