#pragma once

#include <vector>
#include <memory>
#include <daedalus/DaedalusGameState.h>

#include "game/constants.h"

class Item;
class World;
class GameScript;
class Npc;
class Interactive;
class Serialize;

class Inventory final {
  public:
    Inventory();
    Inventory(Inventory&&)=default;
    ~Inventory();

    void load(Npc& owner, Serialize& s);
    void load(Interactive& owner, World &w, Serialize& s);
    void save(Serialize& s) const;

    enum Flags : uint32_t {
      ITM_CAT_NONE   = 1 << 0,
      ITM_CAT_NF     = 1 << 1,
      ITM_CAT_FF     = 1 << 2,
      ITM_CAT_MUN    = 1 << 3,
      ITM_CAT_ARMOR  = 1 << 4,
      ITM_CAT_FOOD   = 1 << 5,
      ITM_CAT_DOCS   = 1 << 6,
      ITM_CAT_POTION = 1 << 7,
      ITM_CAT_LIGHT  = 1 << 8,
      ITM_CAT_RUNE   = 1 << 9,
      ITM_CAT_MAGIC  = 1u << 31,
      ITM_10         = 1 << 10, // ???
      ITM_RING       = 1 << 11,
      ITM_MISSION    = 1 << 12,
      ITM_DAG        = 1 << 13,
      ITM_SWD        = 1 << 14,
      ITM_AXE        = 1 << 15,
      ITM_2HD_SWD    = 1 << 16,
      ITM_2HD_AXE    = 1 << 17,
      ITM_SHIELD     = 1 << 18,
      ITM_BOW        = 1 << 19,
      ITM_CROSSBOW   = 1 << 20,
      ITM_MULTI      = 1 << 21,
      ITM_AMULET     = 1 << 22,
      ITM_BELT       = 1 << 24,
      ITM_TORCH	     = 1 << 28
      };

    int32_t      priceOf(size_t item) const;
    int32_t      sellPriceOf(size_t item) const;
    size_t       goldCount() const;
    size_t       itemCount(const size_t id) const;
    size_t       recordsCount()  const { return items.size(); }
    size_t       tradableCount() const;
    size_t       ransackCount() const;
    const Item&  at(size_t i) const;
    Item&        at(size_t i);
    const Item&  atTrade(size_t i) const;
    const Item&  atRansack(size_t i) const;
    static void  trasfer(Inventory& to, Inventory& from, Npc *fromNpc, size_t cls, uint32_t count, World &wrld);

    Item*  getItem(size_t instance);
    Item*  addItem(std::unique_ptr<Item>&& p);
    Item*  addItem(const char* name, uint32_t count, World &owner);
    Item*  addItem(size_t cls, uint32_t count, World &owner);
    void   delItem(size_t cls, uint32_t count, Npc &owner);
    bool   use    (size_t cls, Npc &owner, bool force);
    bool   equip  (size_t cls, Npc &owner, bool force);
    bool   unequip(size_t cls, Npc &owner);
    void   unequip(Item*  cls, Npc &owner);
    void   invalidateCond(Npc &owner);
    bool   isChanged() const { return !sorted; }
    void   autoEquip(Npc &owner);
    void   equipArmour         (int32_t cls, Npc &owner);
    void   equipBestArmour     (Npc &owner);
    void   equipBestMeleWeapon (Npc &owner);
    void   equipBestRangeWeapon(Npc &owner);
    void   unequipWeapons(GameScript &vm, Npc &owner);
    void   unequipArmour(GameScript &vm, Npc &owner);
    void   clear(GameScript &vm, Npc &owner);

    void   updateArmourView(Npc& owner);
    void   updateSwordView (Npc& owner);
    void   updateBowView   (Npc& owner);
    void   updateRuneView  (Npc& owner);

    const Item*  activeWeapon() const;
    Item*  activeWeapon();
    void   switchActiveWeaponFist();
    void   switchActiveWeapon(Npc &owner, uint8_t slot);
    void   switchActiveSpell (int32_t spell, Npc &owner);

    Item*  currentArmour()         { return armour;     }
    Item*  currentMeleWeapon()     { return mele;       }
    Item*  currentRangeWeapon()    { return range;      }
    Item*  currentSpell(uint8_t s) { return numslot[s]; }
    const Item*  currentSpell(uint8_t s) const { return numslot[s]; }

    auto   weaponState() const -> WeaponState;
    uint8_t currentSpellSlot() const;

    void   putCurrentToSlot(Npc& owner, const char* slot);
    void   putToSlot       (Npc& owner, size_t cls, const char* slot);
    void   clearSlot       (Npc& owner, const char* slot, bool remove);

    void   setCurrentItem(size_t cls);
    void   setStateItem  (size_t cls);

  private:
    struct MdlSlot final {
      std::string slot;
      Item*       item = nullptr;
      };

    void   implLoad(Npc *owner, World &world, Serialize& s);

    bool   setSlot     (Item*& slot, Item *next, Npc &owner, bool force);
    bool   equipNumSlot(Item *next, Npc &owner, bool force);
    void   applyArmour (Item& it, Npc &owner, int32_t sgn);

    Item*  findByClass(size_t cls);
    void   delItem    (Item* it, uint32_t count, Npc& owner);
    void   invalidateCond(Item*& slot,  Npc &owner);

    Item*  bestItem       (Npc &owner, Flags f);
    Item*  bestArmour     (Npc &owner);
    Item*  bestMeleeWeapon(Npc &owner);
    Item*  bestRangeWeapon(Npc &owner);

    bool   isTakable(const Item& i) const;
    void   applyWeaponStats(Npc &owner, const Item& weap, int sgn);

    void        sortItems() const;
    static bool less(Item& l,Item& r);
    static auto orderId(Item& l) -> std::pair<int,int>;
    uint8_t     slotId(Item*& slt) const;

    mutable std::vector<std::unique_ptr<Item>> items;
    mutable bool                               sorted=false;

    uint32_t                           indexOf(const Item* it) const;
    Item*                              readPtr(Serialize& fin);

    Item*                              armour=nullptr;
    Item*                              belt  =nullptr;
    Item*                              amulet=nullptr;
    Item*                              ringL =nullptr;
    Item*                              ringR =nullptr;

    Item**                             active=nullptr;
    Item*                              mele  =nullptr;
    Item*                              range =nullptr;
    Item*                              numslot[8]={};
    std::vector<MdlSlot>               mdlSlots;
    int32_t                            curItem=0;
    int32_t                            stateItem=0;
  };
