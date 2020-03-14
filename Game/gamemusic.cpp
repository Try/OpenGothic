#include "gamemusic.h"

#include <Tempest/Sound>
#include <Tempest/Log>

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
    Daedalus::GEngineClasses::C_MusicTheme theme;
    bool                                   updateTheme=false;
    bool                                   reloadTheme=false;
    Tags                                   tags=Tags::Day;

    {
      std::lock_guard<std::mutex> guard(pendingSync);
      if(hasPending) {
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
        Dx8::PatternList p = Resources::loadDxMusic(theme.file.c_str());

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
      Log::e("unable to load sound: \"",theme.file.c_str(),"\"");
      }
    }

  bool setMusic(const Daedalus::GEngineClasses::C_MusicTheme &theme, Tags tags){
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
    }

  void stopMusic() {
    std::lock_guard<std::mutex> guard(pendingSync);
    mix.setMusic(Dx8::Music());
    }

  void setVolume(float v) {
    mix.setVolume(v);
    }

  Dx8::Mixer                             mix;

  std::mutex                             pendingSync;
  bool                                   hasPending=false;
  bool                                   reloadTheme=false;
  Daedalus::GEngineClasses::C_MusicTheme pendingMusic;
  Tags                                   pendingTags=Tags::Day;
  Tags                                   currentTags=Tags::Day;
  };

struct GameMusic::Impl final {
  Impl() {
    std::unique_ptr<MusicProducer> mix(new MusicProducer());
    dxMixer = mix.get();
    sound   = device.load(std::move(mix));

    dxMixer->setVolume(masterVolume);
    }

  void setMusic(const Daedalus::GEngineClasses::C_MusicTheme &theme, Tags tags) {
    dxMixer->setMusic(theme,tags);
    }

  void startMusic() {
    sound.play();
    dxMixer->restartMusic();
    }

  void stopMusic() {
    dxMixer->stopMusic();
    }

  Tempest::SoundDevice                          device;
  Tempest::SoundEffect                          sound;

  MusicProducer*                                dxMixer=nullptr;
  float                                         masterVolume=0.5f;
  bool                                          enableMusic=true;
  };

GameMusic::GameMusic() {
  impl.reset(new Impl());
  }

GameMusic::~GameMusic() {
  }

GameMusic::Tags GameMusic::mkTags(GameMusic::Tags daytime, GameMusic::Tags mode) {
  return Tags(daytime|mode);
  }

void GameMusic::setEnabled(bool e) {
  impl->enableMusic = e;
  if(!e)
    impl->stopMusic(); else
    impl->startMusic();
  }

bool GameMusic::isEnabled() const {
  return impl->enableMusic;
  }

void GameMusic::setMusic(const Daedalus::GEngineClasses::C_MusicTheme &theme, Tags tags) {
  if(!impl->enableMusic)
    return;
  impl->setMusic(theme,tags);
  }

void GameMusic::stopMusic() {
  impl->stopMusic();
  }
