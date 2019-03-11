#pragma once

#include <daedalus/DaedalusVM.h>

#include "graphics/staticobjects.h"

class WorldScript;
class Npc;

class Item final {
  public:
    Item(WorldScript& owner,Daedalus::GameState::ItemHandle hnpc);
    Item(Item&&)=default;
    Item& operator=(Item&&)=default;

    enum { MAX_UI_ROWS=6, NSLOT=255 };

    void setView      (StaticObjects::Mesh&& m);
    void setPosition  (float x,float y,float z);
    void setDirection (float x,float y,float z);

    void setMatrix(const Tempest::Matrix4x4& m);

    bool isEquiped() const    { return equiped; }
    void setAsEquiped(bool e) { equiped=e; if(!e) itSlot=NSLOT; }

    uint8_t slot() const      { return itSlot;  }
    void setSlot(uint8_t s)   { itSlot = s;     }

    const char*         displayName() const;
    const char*         description() const;
    std::array<float,3> position() const;
    bool                isGold() const;
    int32_t             mainFlag() const;
    int32_t             itemFlag() const;

    bool                isSpell() const;
    bool                is2H() const;
    bool                isCrossbow() const;
    int32_t             spellId() const;

    const char*         uiText (size_t id) const;
    int32_t             uiValue(size_t id) const;
    size_t              count() const;
    int32_t             cost() const;
    int32_t             sellCost() const;

    bool                checkCond    (const Npc& other) const;
    bool                checkCondUse (const Npc& other,int32_t& atr,int32_t& nv) const;
    bool                checkCondRune(const Npc& other,int32_t& cPl,int32_t& cIt) const;

    Daedalus::GameState::ItemHandle handle() const { return hitem; }
    size_t                          clsId() const;

  private:
    void updateMatrix();

    WorldScript&                    owner;
    Daedalus::GameState::ItemHandle hitem={};
    StaticObjects::Mesh             view;
    std::array<float,3>             pos={};
    bool                            equiped=false;
    uint8_t                         itSlot=NSLOT;
  };
