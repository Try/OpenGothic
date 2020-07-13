#pragma once

#include "graphics/meshobjects.h"
#include "physics/physicmesh.h"
#include "abstracttrigger.h"

class MoveTrigger : public AbstractTrigger {
  public:
    MoveTrigger(Vob* parent, World &world, ZenLoad::zCVobData&& data, bool startup);

    void onTrigger(const TriggerEvent& evt) override;
    bool hasVolume() const override;
    void tick(uint64_t dt) override;

  private:
    void moveEvent() override;
    void setView     (MeshObjects::Mesh&& m);
    void emitSound   (const char* snd, bool freeSlot=true);

    enum Anim {
      IdleClosed,
      IdleOpenned,
      Close,
      Open,
      };

    Tempest::Matrix4x4       pos0;
    MeshObjects::Mesh        view;
    PhysicMesh               physic;

    Anim                     anim  = IdleClosed;
    uint32_t                 frame = 0;
    uint64_t                 sAnim = 0;
  };
