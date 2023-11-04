#pragma once

#include <phoenix/vobs/camera.hh>
#include "abstracttrigger.h"

class World;

class CsCamera : public AbstractTrigger {
  public:
    CsCamera(Vob* parent, World& world, const phoenix::vobs::cs_camera& data, Flags flags);

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

    void clear();

    auto position() -> Tempest::Vec3;
    auto spin(Tempest::Vec3& d) -> Tempest::PointF;

    bool     active       = false;
    bool     godMode;
    float    duration;
    float    delay;
    float    time;
    KbSpline posSpline    = {};
    KbSpline targetSpline = {};
  };
