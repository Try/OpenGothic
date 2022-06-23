#pragma once

#include "graphics/meshobjects.h"
#include "physics/physicmesh.h"
#include "abstracttrigger.h"

class MoveTrigger : public AbstractTrigger {
  public:
    MoveTrigger(Vob* parent, World &world, ZenLoad::zCVobData&& data, Flags flags);

    void save(Serialize& fout) const override;
    void load(Serialize &fin) override;

    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
    void onGotoMsg(const TriggerEvent& evt) override;

    bool hasVolume() const override;
    void tick(uint64_t dt) override;

  private:
    void moveEvent() override;
    void processTrigger(const TriggerEvent& evt, bool onTrigger);

    void setView     (MeshObjects::Mesh&& m);
    void emitSound   (std::string_view snd, bool freeSlot=true);
    void advanceAnim (uint32_t f0, uint32_t f1, float alpha);

    float pathLength() const;
    void invalidateView();

    enum State : int32_t {
      Idle         = 0,
      Loop         = 1,
      Open         = 2,
      OpenTimed    = 3,
      Close        = 4,
      NextKey      = 5,
      };

    struct KeyLen {
      float    position = 0;
      uint64_t ticks    = 0;
      };

    Tempest::Matrix4x4       pos0;
    MeshObjects::Mesh        view;
    PhysicMesh               physic;
    std::vector<KeyLen>      keyframes;

    State                    state     = Idle;
    uint64_t                 sAnim     = 0;

    uint32_t                 frame     = 0;
  };
