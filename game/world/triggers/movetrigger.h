#pragma once

#include <zenkit/vobs/Trigger.hh>

#include "graphics/meshobjects.h"
#include "physics/physicmesh.h"
#include "abstracttrigger.h"

class MoveTrigger : public AbstractTrigger {
  public:
    MoveTrigger(Vob* parent, World &world, const zenkit::VMover& data, Flags flags);

    void save(Serialize& fout) const override;
    void load(Serialize &fin) override;

    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
    void onGotoMsg(const TriggerEvent& evt) override;

    void tick(uint64_t dt) override;

  private:
    enum State : int32_t {
      Idle         = 0,
      Loop         = 1,
      Open         = 2,
      OpenTimed    = 3,
      Close        = 4,
      SingleKey    = 5,
      };

    struct KeyLen {
      uint64_t ticks = 0;
      };

    void     moveEvent() override;
    void     preProcessTrigger(State prev = Idle);
    void     postProcessTrigger();

    void     setView(MeshObjects::Mesh&& m);
    void     emitSound(std::string_view snd, bool freeSlot=true) const;
    void     advanceAnim();
    float    calcProgress(uint32_t f1, uint32_t f2) const;
    auto     calcTransform(uint32_t f1, uint32_t f2, float a) const -> Tempest::Matrix4x4;
    void     updateFrame();
    uint32_t nextFrame(uint32_t i) const;
    uint32_t prevFrame(uint32_t i) const;
    float    scaleRotSpeed(float speed) const;

    void     invalidateView();

    Tempest::Matrix4x4                   pos0;
    MeshObjects::Mesh                    view;
    PhysicMesh                           physic;
    std::vector<KeyLen>                  keyframes;
    std::vector<zenkit::AnimationSample> moverKeyFrames;
    zenkit::MoverBehavior                behavior;
    zenkit::MoverSpeedType               speedType;
    zenkit::MoverLerpType                lerpType;
    bool                                 autoRotate;
    std::string                          sfxOpenStart;
    std::string                          sfxOpenEnd;
    std::string                          sfxCloseStart;
    std::string                          sfxCloseEnd;
    std::string                          sfxMoving;
    std::string                          visualName;
    uint64_t                             stayOpenTime;

    State                                state       = Idle;
    uint64_t                             frameTime   = 0;
    uint32_t                             frame       = 0;
    uint32_t                             targetFrame = uint32_t(-1);
  };
