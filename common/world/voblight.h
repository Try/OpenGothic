#pragma once

#include "world/objects/vob.h"
#include "graphics/lightgroup.h"

class VobLight : public Vob {
  public:
    VobLight(Vob* parent, World& world, const zenkit::VLight& vob, Flags flags);

  private:
    void  moveEvent() override;

    LightGroup::Light light;
  };

