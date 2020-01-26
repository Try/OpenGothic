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

    {
      std::lock_guard<std::mutex> guard(pendingSync);
      if(hasPending) {
        hasPending  = false;
        updateTheme = true;
        theme       = pendingMusic;
        }
    }

    if(!updateTheme)
      return;
    updateTheme = false;

    try {
      //Dx8::PatternList m = Resources::loadDxMusic("OWD_DayStd.sgt");
      Dx8::PatternList p = Resources::loadDxMusic(theme.file.c_str());

      Dx8::Music m;
      for(size_t i=0;i<p.size();++i) {
        auto& pat = p[i];
        if(pat.name.find("Std")!=std::string::npos)
          m.addPattern(p,i);
        }
      if(m.size()==0)
        m.addPattern(p);

      m.setVolume(theme.vol);
      mix.setMusic(m);
      }
    catch(std::runtime_error&) {
      Log::e("unable to load sound: ",theme.file.c_str());
      }
    }

  bool setMusic(const Daedalus::GEngineClasses::C_MusicTheme &theme){
    std::lock_guard<std::mutex> guard(pendingSync);
    if(pendingMusic.file==theme.file)
      return false;
    pendingMusic = theme;
    hasPending   = true;
    return true;
    }

  void stopMusic() {
    std::lock_guard<std::mutex> guard(pendingSync);
    mix.setMusic(Dx8::Music());
    }

  void setVolume(float v) {
    mix.setVolume(v);
    }

  Dx8::Mixer                                    mix;

  std::mutex                                    pendingSync;
  bool                                          hasPending=false;
  Daedalus::GEngineClasses::C_MusicTheme        pendingMusic;
  };

struct GameMusic::Impl final {
  Impl() {
    std::unique_ptr<MusicProducer> mix(new MusicProducer());
    dxMixer = mix.get();
    sound   = device.load(std::move(mix));

    dxMixer->setVolume(masterVolume);
    }

  void setMusic(const Daedalus::GEngineClasses::C_MusicTheme &theme) {
    if(!dxMixer->setMusic(theme))
      return;
    sound.play();
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

void GameMusic::setEnabled(bool e) {
  impl->enableMusic = e;
  if(!e)
    impl->stopMusic();
  }

bool GameMusic::isEnabled() const {
  return impl->enableMusic;
  }

void GameMusic::setMusic(const Daedalus::GEngineClasses::C_MusicTheme &theme,const char*) {
  if(!impl->enableMusic)
    return;
  impl->setMusic(theme);
  }

void GameMusic::stopMusic() {
  impl->stopMusic();
  }
