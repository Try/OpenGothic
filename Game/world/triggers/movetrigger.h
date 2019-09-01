#pragma once

#include "graphics/staticobjects.h"
#include "physics/dynamicworld.h"
#include "abstracttrigger.h"

class MoveTrigger : public AbstractTrigger {
  public:
    MoveTrigger(ZenLoad::zCVobData&& data, World &owner);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    void setView     (StaticObjects::Mesh&& m);
    void setPhysic   (DynamicWorld::StaticItem&&  p);
    void setObjMatrix(const Tempest::Matrix4x4 &m);

    StaticObjects::Mesh      view;
    DynamicWorld::StaticItem physic;
    uint32_t                 frame=0;
    Tempest::Matrix4x4       pos;
  };
