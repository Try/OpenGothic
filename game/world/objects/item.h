#pragma once

#include <daedalus/DaedalusVM.h>

#include "graphics/objvisual.h"
#include "graphics/meshobjects.h"
#include "physics/dynamicworld.h"
#include "vob.h"

class World;
class Npc;
class Serialize;

class Item final : public Vob {
  public:
    enum { MAX_UI_ROWS=6, NSLOT=255 };

    Item(World& owner, size_t     inst, bool inWorld);
    Item(World& owner, Serialize& fin,  bool inWorld);
    Item(Item&&);
    ~Item();
    Item& operator=(Item&&)=delete;

    void    save(Serialize& fout) const override;

    void    clearView();

    void    setPosition  (float x,float y,float z);
    void    setDirection (float x,float y,float z);
    void    setMatrix(const Tempest::Matrix4x4& m);

    bool    isMission() const;
    bool    isEquiped() const  { return equiped>0; }
    uint8_t equipCount() const { return equiped;   }
    void    setAsEquiped(bool e);

    void    setPhysicsEnable (DynamicWorld& physic);
    void    setPhysicsDisable();
    bool    isDynamic() const override;

    uint8_t slot() const       { return itSlot;  }
    void    setSlot(uint8_t s) { itSlot = s;     }

    std::string_view    displayName() const;
    std::string_view    description() const;
    Tempest::Vec3       position() const;
    bool                isGold() const;
    ItmFlags            mainFlag() const;
    int32_t             itemFlag() const;

    bool                isMulti() const;
    bool                is2H() const;
    bool                isCrossbow() const;
    bool                isRing() const;
    bool                isArmour() const;
    bool                isSpellShoot() const;
    bool                isSpellOrRune() const;
    bool                isSpell() const;
    bool                isRune() const;
    int32_t             spellId() const;
    int32_t             swordLength() const;

    void                setCount(size_t cnt);
    size_t              count() const;

    std::string_view    uiText(size_t id) const;
    int32_t             uiValue(size_t id) const;
    int32_t             cost() const;
    int32_t             sellCost() const;

    bool                checkCond    (const Npc& other) const;
    bool                checkCondUse (const Npc& other,int32_t& atr,int32_t& nv) const;
    bool                checkCondRune(const Npc& other,int32_t& cPl,int32_t& cIt) const;

    const Daedalus::GEngineClasses::C_Item& handle() const { return hitem; }
    Daedalus::GEngineClasses::C_Item&       handle()       { return hitem; }
    size_t                                  clsId() const;

  private:
    void                updateMatrix();
    void                moveEvent() override;

    Daedalus::GEngineClasses::C_Item  hitem={};
    MeshObjects::Mesh                 view;
    Tempest::Vec3                     pos={};

    uint32_t                          amount  = 0;
    uint8_t                           equiped = 0;
    uint8_t                           itSlot  = NSLOT;

    DynamicWorld::Item                physic;
  };
