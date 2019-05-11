#pragma once

#include <Tempest/SoundEffect>
#include <Tempest/Sound>
#include <vector>

#include <daedalus/DaedalusStdlib.h>

#include "gsoundeffect.h"

class GameSession;

class SoundFx {
  public:
    SoundFx(GameSession& gothic,const char *tagname);
    SoundFx(SoundFx&&)=default;
    SoundFx& operator=(SoundFx&&)=default;

    GSoundEffect getEffect(Tempest::SoundDevice& dev);

  private:
    struct SoundVar {
      SoundVar()=default;
      SoundVar(const Daedalus::GEngineClasses::C_SFX& sfx,Tempest::Sound&& snd);

      Tempest::Sound snd;
      float          vol = 0.5f;
      };

    std::vector<SoundVar> inst;
    void loadVariants(GameSession &gothic, const char* name);
  };

