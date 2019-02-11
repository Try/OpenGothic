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
    bool isFaling() const;
    bool isFrozen() const;

  private:
    Npc&         npc;
    const World& world;

    enum Flags : uint32_t {
      NoFlags=0,
      Faling =1,
      Frozen =1<<1,
      };

    float  mulSpeed =1.f;
    float  pSpeed   =0.f;
    float  fallSpeed=0.f;
    Flags  flags    =NoFlags;

    void   setAsFaling(bool f);
    void   setAsFrozen(bool f);

    void setPos(std::array<float,3> pos, uint64_t dt, float speed);
  };
