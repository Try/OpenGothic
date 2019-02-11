#pragma once

#include <cstdint>
#include <array>

class Npc;
class World;

class MoveAlgo final {
  public:
    MoveAlgo(Npc& unit,const World& w);

    void tick(uint64_t dt);
    void multSpeed(float s){ mulSpeed=s; }

  private:
    Npc&         npc;
    const World& world;

    float  mulSpeed =1.f;
    float  pSpeed   =0.f;
    float  fallSpeed=0.f;

    void setPos(std::array<float,3> pos, uint64_t dt, float speed);
  };
