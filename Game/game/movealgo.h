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
    bool isInAir()  const;

  private:
    enum Flags : uint32_t {
      NoFlags=0,
      Frozen =1<<1,
      InAir  =1<<2,
      Faling =1<<3,
      Slide  =1<<4,
      };

    void   setAsFrozen(bool f);
    void   setInAir(bool f);
    void   setSllideFaling(bool slide,bool faling);

    void   setPos(std::array<float,3> pos, uint64_t dt, float speed);
    bool   trySlide(std::array<float,3> &pos, std::array<float,3> &norm);

    Npc&                npc;
    const World&        world;

    float               mulSpeed  =1.f;
    //float               pSpeed    =0.f;
    std::array<float,3> aniSpeed={};
    std::array<float,3> fallSpeed={};
    Flags               flags=NoFlags;

    static const float slideBegin;
    static const float fallThreshold;
  };
