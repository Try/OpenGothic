#pragma once

#include <daedalus/DaedalusVM.h>

#include "graphics/meshobjects.h"
#include "vob.h"

class World;
class Npc;
class Serialize;

class Item final : public Vob {
  public:
    Item(World& owner, size_t inst);
    Item(World& owner, Serialize& fin, bool inWorld);
    Item(Item&&);
    ~Item();
    Item& operator=(Item&&)=delete;

    void save(Serialize& fout);

    enum { MAX_UI_ROWS=6, NSLOT=255 };

    void setView      (MeshObjects::Mesh&& m);
    void clearView    ();

    void setPosition  (float x,float y,float z);
    void setDirection (float x,float y,float z);
    void setMatrix(const Tempest::Matrix4x4& m);

    bool isMission() const;
    bool isEquiped() const    { return equiped; }
    void setAsEquiped(bool e) { equiped=e; if(!e) itSlot=NSLOT; }

    uint8_t slot() const      { return itSlot;  }
    void    setSlot(uint8_t s)   { itSlot = s;     }

    const char*         displayName() const;
    const char*         description() const;
    Tempest::Vec3       position() const;
    bool                isGold() const;
    int32_t             mainFlag() const;
    int32_t             itemFlag() const;

    bool                isSpellShoot() const;
    bool                isSpellOrRune() const;
    bool                isSpell() const;
    bool                is2H() const;
    bool                isCrossbow() const;
    int32_t             spellId() const;
    int32_t             swordLength() const;

    void                setCount(size_t cnt);
    size_t              count() const;

    const char*         uiText (size_t id) const;
    int32_t             uiValue(size_t id) const;
    int32_t             cost() const;
    int32_t             sellCost() const;

    bool                checkCond    (const Npc& other) const;
    bool                checkCondUse (const Npc& other,int32_t& atr,int32_t& nv) const;
    bool                checkCondRune(const Npc& other,int32_t& cPl,int32_t& cIt) const;

    const Daedalus::GEngineClasses::C_Item* handle() const { return &hitem; }
    Daedalus::GEngineClasses::C_Item*       handle() { return &hitem; }
    size_t                                  clsId() const;

  private:
    void updateMatrix();

    Daedalus::GEngineClasses::C_Item  hitem={};
    MeshObjects::Mesh                 view;
    Tempest::Vec3                     pos={};
    bool                              equiped=false;
    uint8_t                           itSlot=NSLOT;
  };
