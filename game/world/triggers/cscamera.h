#pragma once

#include <phoenix/vobs/camera.hh>
#include "abstracttrigger.h"

class World;

class CsCamera : public AbstractTrigger {
  public:
    CsCamera(Vob* parent, World& world, const phoenix::vobs::cs_camera& data, Flags flags);

  private:
    struct KeyFrame {
      float         time  = 0;
      Tempest::Vec3 c[4]  = {};
      float arcLength(float t0 = 0, float t1 = 1);
      };

    struct CamSpline {
      float                 t;
      float                 dist;
      float                 c[3]     = {};
      std::vector<KeyFrame> keyframe = {};
      uint32_t size() { return uint32_t(keyframe.size()); }
      float    nextDist(float t);
      KeyFrame& operator [](const size_t n) { return keyframe[n]; }
      };

    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
    void tick(uint64_t dt) override;

    void init(const phoenix::vobs::cs_camera& cam);
    void clear();

    auto position(CamSpline& cs) -> Tempest::Vec3;
    auto spin(const Tempest::Vec3& d) -> Tempest::PointF;

    static inline uint8_t   activeEvents;
                  bool      active       = false;
                  bool      godMode;

                  float     duration;
                  float     delay;
                  float     time;
                  CamSpline posSpline    = {};
                  CamSpline targetSpline = {};
  };
