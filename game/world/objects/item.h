#pragma once

#include <phoenix/ext/daedalus_classes.hh>

#include "graphics/objvisual.h"
#include "graphics/meshobjects.h"
#include "physics/dynamicworld.h"
#include "vob.h"

class World;
class Npc;
class Serialize;

class Item : public Vob {
  public:
    enum { MAX_UI_ROWS=6, NSLOT=255 };

    enum Type : uint8_t {
      T_World,
      T_WorldDyn,
      T_Inventory,
      };

    Item(World& owner, size_t     inst, Type type);
    Item(World& owner, Serialize& fin,  Type type);
    Item(Item&&);
    ~Item();
    Item& operator=(Item&&)=delete;

    void    save(Serialize& fout) const override;

    virtual void clearView();
    virtual bool isTorchBurn() const;

    void    setPosition  (float x,float y,float z);
    void    setDirection (float x,float y,float z);
    void    setObjMatrix (const Tempest::Matrix4x4& m);

    bool    isMission() const;
    bool    isEquiped() const  { return equiped>0; }
    uint8_t equipCount() const { return equiped;   }
    void    setAsEquiped(bool e);

    void    setPhysicsEnable (World& w);
    void    setPhysicsDisable();
    bool    isDynamic() const override;

    uint8_t slot() const       { return itSlot;  }
    void    setSlot(uint8_t s) { itSlot = s;     }

    std::string_view    displayName() const;
    std::string_view    description() const;
    Tempest::Vec3       position() const;
    Tempest::Vec3       midPosition() const;
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

    const phoenix::c_item&                   handle() const { return *hitem; }
    phoenix::c_item&                         handle() { return *hitem; }
    const std::shared_ptr<phoenix::c_item>&  handlePtr()    { return hitem; }
    size_t                                   clsId() const;

  protected:
    void                moveEvent() override;
    void                setPhysicsEnable(const MeshObjects::Mesh& mesh);
    void                setPhysicsEnable(const ProtoMesh* mesh);

  private:
    void                updateMatrix();

    std::shared_ptr<phoenix::c_item> hitem={};
    Tempest::Vec3                    pos={};

    uint32_t                          amount  = 0;
    uint8_t                           equiped = 0;
    uint8_t                           itSlot  = NSLOT;

    MeshObjects::Mesh                 view;
    DynamicWorld::Item                physic;
  };
