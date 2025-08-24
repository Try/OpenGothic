#pragma once

#include <zenkit/vobs/Camera.hh>

#include "abstracttrigger.h"

class World;
class DbgPainter;

class CsCamera : public AbstractTrigger {
  public:
    CsCamera(Vob* parent, World& world, const zenkit::VCutsceneCamera& data, Flags flags);

    bool isPlayerMovable() const;

    void debugDraw(DbgPainter& p) const;

  private:
    struct KeyFrame {
      float         time = 0;
      Tempest::Vec3 c[4] = {};
      zenkit::CameraMotion motionType = zenkit::CameraMotion::LINEAR;

      auto          position(float u) const -> Tempest::Vec3;
      float         arcLength() const;
      };

    struct KbSpline {
      KbSpline() = default;
      KbSpline(const std::vector<std::shared_ptr<zenkit::VCameraTrajectoryFrame>>& frames, const float duration, std::string_view vobName);

      std::vector<KeyFrame> keyframe;
      size_t                size() const { return keyframe.size(); }
      auto                  position(const uint64_t time) const -> Tempest::Vec3;
      };

    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
    void tick(uint64_t dt) override;

    auto position() -> Tempest::Vec3;
    auto spin(Tempest::Vec3& d) -> Tempest::PointF;

    KbSpline posSpline     = {};
    KbSpline targetSpline  = {};

    uint64_t duration      = 0;
    uint64_t delay         = 0;
    uint64_t timeTrigger   = 0;

    bool     active        = false;
    bool     playerMovable = false;
    bool     autoUntrigger = false;
  };
