#pragma once

#include <cstdint>

#include "physics/dynamicworld.h"

#include "graphics/meshobjects.h"
#include "graphics/pfxobjects.h"

class World;
class Item;
class Npc;

class Bullet final : public DynamicWorld::BulletCallback {
  public:
    Bullet()=default;
    Bullet(World &owner, const Item &itm, float x, float y, float z);
    Bullet(Bullet&&)=default;
    ~Bullet() override;
    Bullet& operator=(Bullet&&)=default;

    enum Flg:uint8_t {
      NoFlags = 0,
      Stopped = 1,
      };

    void setPosition  (const Tempest::Vec3& p);
    void setPosition  (float x,float y,float z);

    void setDirection (float x,float y,float z);

    void setView      (MeshObjects::Mesh&&   m);
    void setView      (PfxObjects::Emitter&& p);

    bool    isSpell() const;
    int32_t spellId() const;

    void setOwner(Npc* n);
    Npc* owner() const;

    Flg                        flags()     const { return flg;  }
    void                       setFlags(Flg f) { flg=f; }
    void                       addFlags(Flg f) { flg=Flg(flg|f); }

    auto                       damage() const -> const std::array<int32_t,Daedalus::GEngineClasses::DAM_INDEX_MAX>& { return dmg; }
    void                       setDamage(std::array<int32_t,Daedalus::GEngineClasses::DAM_INDEX_MAX> d) { dmg=d; }

    float                      hitChance() const { return hitCh; }
    void                       setHitChance(float h) { hitCh=h; }

    float                      pathLength() const;

  protected:
    void                       onStop() override;
    void                       onMove() override;
    void                       onCollide(uint8_t matId) override;
    void                       onCollide(Npc& other) override;
    void                       collideCommon();

  private:
    DynamicWorld::BulletBody*         obj=nullptr;
    World*                            wrld=nullptr;
    Npc*                              ow=nullptr;

    std::array<int32_t,Daedalus::GEngineClasses::DAM_INDEX_MAX> dmg={};
    float                             hitCh=1.f;

    MeshObjects::Mesh                 view;
    PfxObjects::Emitter               pfx;

    uint8_t                           material=0;
    Flg                               flg=NoFlags;

    void updateMatrix();
  };

