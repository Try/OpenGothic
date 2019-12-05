#pragma once

#include <cstdint>

#include "graphics/meshobjects.h"
#include "graphics/pfxobjects.h"

class World;
class Item;
class Npc;

class Bullet final {
  public:
    Bullet(World &owner, const Item &itm);
    Bullet(Bullet&&)=default;
    ~Bullet();
    Bullet& operator=(Bullet&&)=default;

    enum Flg:uint8_t {
      NoFlags = 0,
      Stopped = 1,
      Spell   = 1<<1,
      };

    void setPosition  (const std::array<float,3>& p);
    void setPosition  (float x,float y,float z);
    void setDirection (float x,float y,float z);
    void setView      (MeshObjects::Mesh&&   m);
    void setView      (PfxObjects::Emitter&& p);

    void setOwner(Npc* n);
    Npc* owner() const;

    const std::array<float,3>& position()  const { return pos;  }
    const std::array<float,3>& direction() const { return dir;  }
    float                      speed()     const { return dirL; }

    Flg                        flags()     const { return flg;  }
    void                       setFlags(Flg f) { flg=f; }
    void                       addFlags(Flg f) { flg=Flg(flg|f); }

    auto                       damage() const -> const std::array<int32_t,Daedalus::GEngineClasses::DAM_INDEX_MAX>& { return dmg; }
    void                       setDamage(std::array<int32_t,Daedalus::GEngineClasses::DAM_INDEX_MAX> d) { dmg=d; }

    float                      hitChance() const { return hitCh; }
    void                       setHitChance(float h) { hitCh=h; }

    void                       addLen(float l) { totalL+=l; }
    float                      pathLength() const { return totalL; }

    bool                       tick(uint64_t dt);

  private:
    World*                            wrld=nullptr;
    Npc*                              ow=nullptr;
    std::array<int32_t,Daedalus::GEngineClasses::DAM_INDEX_MAX> dmg={};

    MeshObjects::Mesh                 view;
    PfxObjects::Emitter               pfx;

    std::array<float,3>               pos={};
    std::array<float,3>               dir={};
    float                             dirL=0.f;
    float                             totalL=0.f;
    float                             hitCh=1.f;
    Tempest::Matrix4x4                mat;
    uint8_t                           material=0;
    Flg                               flg=NoFlags;

    void updateMatrix();
  };

