#pragma once

#include <Tempest/SoundEffect>
#include <Tempest/Sound>
#include <vector>

class SoundFx {
  public:
    SoundFx(const char *tagname);
    SoundFx(SoundFx&&)=default;

    Tempest::SoundEffect getEffect(Tempest::SoundDevice& dev);

  private:
    std::vector<Tempest::Sound> inst;
  };

