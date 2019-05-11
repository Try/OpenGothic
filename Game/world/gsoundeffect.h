#pragma once

#include <Tempest/SoundEffect>

class GSoundEffect final {
  public:
    GSoundEffect()=default;
    GSoundEffect(Tempest::SoundEffect&& eff):eff(std::move(eff)){}

    std::array<float,3> position() const { return eff.position(); }

    bool isEmpty()    const { return eff.isEmpty();    }
    bool isFinished() const { return eff.isFinished(); }

    void setOcclusion(float occ);
    void setVolume(float v);

    void setMaxDistance(float v) { return eff.setMaxDistance(v); }
    void setRefDistance(float v) { return eff.setRefDistance(v); }

    void setPosition(float x,float y,float z) { eff.setPosition(x,y,z); }

    void play(){ eff.play(); }

  private:
    Tempest::SoundEffect eff;
    float                vol=1.f;
    float                occ=1.f;
  };

