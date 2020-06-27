#pragma once

#include <Tempest/SoundEffect>
#include <Tempest/Vec>

class GSoundEffect final {
  public:
    GSoundEffect()=default;
    GSoundEffect(Tempest::SoundEffect&& eff);

    Tempest::Vec3 position() const { return pos; }

    bool isEmpty()    const { return eff.isEmpty();    }
    bool isFinished() const { return eff.isFinished(); }

    void setOcclusion(float occ);
    void setVolume(float v);

    void setMaxDistance(float v) { return eff.setMaxDistance(v); }
    void setRefDistance(float v) { return eff.setRefDistance(v); }

    void setPosition(float x,float y,float z) { eff.setPosition(x,y,z); pos = Tempest::Vec3(x,y,z); }

    void play(){ eff.play(); }

  private:
    Tempest::SoundEffect eff;
    Tempest::Vec3        pos;
    float                vol=1.f;
    float                occ=1.f;
  };

