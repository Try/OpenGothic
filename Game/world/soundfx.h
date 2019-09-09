#pragma once

#include <Tempest/SoundEffect>
#include <Tempest/Sound>
#include <vector>

#include <daedalus/DaedalusStdlib.h>

#include "gsoundeffect.h"

class Gothic;

class SoundFx {
  public:
    SoundFx(Gothic &gothic, const char *tagname);
    SoundFx(Gothic &gothic, Tempest::Sound &&raw);
    SoundFx(SoundFx&&)=default;
    SoundFx& operator=(SoundFx&&)=default;

    GSoundEffect         getEffect(Tempest::SoundDevice& dev) const;
    Tempest::SoundEffect getGlobal(Tempest::SoundDevice& dev) const;

  private:
    struct SoundVar {
      SoundVar()=default;
      SoundVar(const Daedalus::GEngineClasses::C_SFX& sfx,Tempest::Sound&& snd);
      SoundVar(const float vol,Tempest::Sound&& snd);

      Tempest::Sound snd;
      float          vol = 0.5f;
      };

    std::vector<SoundVar> inst;
    void loadVariants(Gothic &gothic, const char* name);
  };

