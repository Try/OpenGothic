#pragma once

#include <cstdint>

#include "graphics/staticobjects.h"

class World;

class Bullet final {
  public:
    Bullet(World& owner, size_t inst);
    Bullet(Bullet&&)=default;
    ~Bullet();
    Bullet& operator=(Bullet&&)=delete;

    void setPosition  (float x,float y,float z);
    void setDirection (float x,float y,float z);
    void setView(StaticObjects::Mesh&& m);

    void tick(uint64_t dt);

  private:
    World&                            owner;
    StaticObjects::Mesh               view;
    std::array<float,3>               pos={};
    std::array<float,3>               dir={};
    float                             ang=0.f;
    Tempest::Matrix4x4                mat;

    void updateMatrix();
  };

