#pragma once

#include "graphics/meshobjects.h"
#include "physics/physicmesh.h"
#include "abstracttrigger.h"

class MoveTrigger : public AbstractTrigger {
  public:
    MoveTrigger(ZenLoad::zCVobData&& data, World &owner);

    void onTrigger(const TriggerEvent& evt) override;
    bool hasVolume() const override;
    void tick(uint64_t dt) override;

  private:
    void setView     (MeshObjects::Mesh&& m);
    void setObjMatrix(const Tempest::Matrix4x4 &m);
    void emitSound   (const char* snd, bool freeSlot=true);

    enum Anim {
      IdleClosed,
      IdleOpenned,
      Close,
      Open,
      };

    MeshObjects::Mesh        view;
    PhysicMesh               physic;
    Tempest::Matrix4x4       pos;

    Anim                     anim  = IdleClosed;
    uint32_t                 frame = 0;
    uint64_t                 sAnim = 0;
  };
