#pragma once

#include <Tempest/SoundEffect>
#include <Tempest/Sound>
#include <vector>

#include <phoenix/ext/daedalus_classes.hh>

class GSoundEffect;

class SoundFx {
  public:
    SoundFx(std::string_view tagname);
    SoundFx(Tempest::Sound &&raw);
    SoundFx(SoundFx&&)=default;
    SoundFx& operator=(SoundFx&&)=default;

    Tempest::SoundEffect getEffect(Tempest::SoundDevice& dev, bool& loop) const;

  private:
    struct SoundVar {
      SoundVar()=default;
      SoundVar(const phoenix::c_sfx& sfx,Tempest::Sound&& snd);
      SoundVar(const float vol,Tempest::Sound&& snd);

      Tempest::Sound snd;
      float          vol  = 0.5f;
      bool           loop = false;
      };

    std::vector<SoundVar> inst;
    void implLoad    (std::string_view name);
    void loadVariants(std::string_view name);
  };

