#pragma once

#include <phoenix/vobs/camera.hh>
#include "abstracttrigger.h"

class World;

class CsCamera : public AbstractTrigger {
  public:
    CsCamera(Vob* parent, World& world, const phoenix::vobs::cs_camera& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;

  private:

  };
