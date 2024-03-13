#pragma once

#include <zenkit/vobs/Camera.hh>

#include "abstracttrigger.h"

class World;

class CsCamera : public AbstractTrigger {
  public:
    CsCamera(Vob* parent, World& world, const zenkit::VCutsceneCamera& data, Flags flags);

    bool isPlayerMovable() const;

  private:
    struct KeyFrame {
      float         time = 0;
      Tempest::Vec3 c[4] = {};
      };

    struct KbSpline {
      float                 c[3]     = {};
      float                 splTime  = 0;
      std::vector<KeyFrame> keyframe;
      size_t size() const { return keyframe.size(); }
      auto   position() const -> Tempest::Vec3;
      void   setSplTime(float t);
      float  applyMotionScaling(float t) const;
      };

    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
    void tick(uint64_t dt) override;

    auto position() -> Tempest::Vec3;
    auto spin(Tempest::Vec3& d) -> Tempest::PointF;

    bool     active       = false;
    bool     godMode      = false;
    float    duration     = 0;
    float    delay        = 0;
    float    time         = 0;
    KbSpline posSpline    = {};
    KbSpline targetSpline = {};
    bool     playerMovable = false;
  };
