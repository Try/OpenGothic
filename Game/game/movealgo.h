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
    bool isSlide()  const;
    bool isFrozen() const;

  private:
    enum Flags : uint32_t {
      NoFlags=0,
      Faling =1,
      Frozen =1<<1,
      Slide  =1<<2,
      };

    void   setAsFaling(bool f);
    void   setAsFrozen(bool f);
    void   setAsSlide (bool f);

    void   setPos(std::array<float,3> pos, uint64_t dt, float speed);
    bool   trySlide(std::array<float,3> &pos, std::array<float,3> &norm);

    Npc&                npc;
    const World&        world;

    float               mulSpeed  =1.f;
    float               pSpeed    =0.f;
    std::array<float,3> fallSpeed;
    Flags               flags=NoFlags;

    static const float slideBegin;
    static const float fallThreshold;
  };
