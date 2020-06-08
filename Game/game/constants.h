#pragma once

#include <cstdint>

enum Guild: uint32_t {
  GIL_NONE                    = 0,  // (keine)
  GIL_HUMAN                   = 1,  // Special Guild -> To set Constants for ALL Human Guilds --> wird verwendet in Species.d
  GIL_PAL                     = 1,  // Paladin
  GIL_MIL                     = 2,  // Miliz
  GIL_VLK                     = 3,  // Bürger
  GIL_KDF                     = 4,  // Magier
  GIL_NOV                     = 5,  // Magier Novize
  GIL_DJG                     = 6,  // Drachenjäger
  GIL_SLD                     = 7,  // Söldner
  GIL_BAU                     = 8,  // Bauer
  GIL_BDT                     = 9,  // Bandit
  GIL_STRF                    = 10, // Prisoner, Sträfling
  GIL_DMT                     = 11, // Dementoren
  GIL_OUT                     = 12, // Outlander (z.B. kleine Bauernhöfe)
  GIL_PIR                     = 13, // Pirat
  GIL_KDW                     = 14, // KDW
  GIL_EMPTY_D                 = 15, // NICHT VERWENDEN!
  //-----------------------------------------------
  GIL_PUBLIC                  = 15,// für öffentliche Portalräume
  //-----------------------------------------------
  GIL_SEPERATOR_HUM           = 16,
  GIL_MEATBUG                 = 17,
  GIL_SHEEP                   = 18,
  GIL_GOBBO                   = 19, // Green Goblin / Black Goblin
  GIL_GOBBO_SKELETON          = 20,
  GIL_SUMMONED_GOBBO_SKELETON = 21,
  GIL_SCAVENGER               = 22, // (bei Bedarf) Scavenger / Evil Scavenger /OrcBiter
  GIL_GIANT_RAT               = 23,
  GIL_GIANT_BUG               = 24,
  GIL_BLOODFLY                = 25,
  GIL_WARAN                   = 26, // Waren / Feuerwaran
  GIL_WOLF                    = 27, // Wolf / Warg
  GIL_SUMMONED_WOLF           = 28,
  GIL_MINECRAWLER             = 29, // Minecrawler / Minecrawler Warrior
  GIL_LURKER                  = 30,
  GIL_SKELETON                = 31,
  GIL_SUMMONED_SKELETON       = 32,
  GIL_SKELETON_MAGE           = 33,
  GIL_ZOMBIE                  = 34,
  GIL_SNAPPER                 = 35, // Snapper / Dragon Snapper /Razor
  GIL_SHADOWBEAST             = 36, //Shadowbeast / Bloodhound
  GIL_SHADOWBEAST_SKELETON    = 37,
  GIL_HARPY                   = 38,
  GIL_STONEGOLEM              = 39,
  GIL_FIREGOLEM               = 40,
  GIL_ICEGOLEM                = 41,
  GIL_SUMMONED_GOLEM          = 42,
  GIL_DEMON                   = 43,
  GIL_SUMMONED_DEMON          = 44,
  GIL_TROLL                   = 45, // Troll / Schwarzer Troll
  GIL_SWAMPSHARK              = 46, // (bei Bedarf)
  GIL_DRAGON                  = 47, // Feuerdrache / Eisdrache / Felsdrache / Sumpfdrache / Untoter Drache
  GIL_MOLERAT                 = 48, // Molerat
  GIL_ALLIGATOR               = 49,
  GIL_SWAMPGOLEM              = 50,
  GIL_Stoneguardian           = 51,
  GIL_Gargoyle                = 52,
  GIL_Empty_A                 = 53,
  GIL_SummonedGuardian        = 54,
  GIL_SummonedZombie          = 55,
  GIL_EMPTY_B                 = 56,
  GIL_EMPTY_C                 = 57,
  GIL_SEPERATOR_ORC           = 58, // (ehem. 37)
  GIL_ORC                     = 59, // Ork-Krieger / Ork-Shamane / Ork-Elite
  GIL_FRIENDLY_ORC            = 60, // Ork-Sklave / Ur-Shak
  GIL_UNDEADORC               = 61,
  GIL_DRACONIAN               = 62,
  GIL_EMPTY_X                 = 63,
  GIL_EMPTY_Y                 = 64,
  GIL_EMPTY_Z	                = 65,
  GIL_MAX                     = 66
  };

enum {
  MAX_AI_USE_DISTANCE=150
  };

enum BodyState:uint32_t {
  BS_NONE                  = 0,

  BS_MOD_HIDDEN            = 1 << 7,
  BS_MOD_DRUNK             = 1 << 8,
  BS_MOD_NUTS              = 1 << 9,
  BS_MOD_BURNING           = 1 << 10,
  BS_MOD_CONTROLLED        = 1 << 11,
  BS_MOD_TRANSFORMED       = 1 << 12,
  BS_FLAG_INTERRUPTABLE    = 1 << 15,
  BS_FLAG_FREEHANDS        = 1 << 16,
  BS_FLAG_OGT_STATEITEM    = 1 << 17,

  BS_MOD_MASK              = BS_MOD_HIDDEN | BS_MOD_DRUNK | BS_MOD_NUTS | BS_MOD_BURNING | BS_MOD_CONTROLLED,
  BS_FLAG_MASK             = BS_FLAG_INTERRUPTABLE | BS_FLAG_FREEHANDS,

  BS_WALK                  = 1  | BS_FLAG_INTERRUPTABLE,
  BS_SNEAK                 = 2  | BS_FLAG_INTERRUPTABLE,
  BS_RUN                   = 3,
  BS_SPRINT                = 4,
  BS_SWIM                  = 5,
  BS_CRAWL                 = 6,
  BS_DIVE                  = 7,
  BS_JUMP                  = 8,
  BS_CLIMB                 = 9  | BS_FLAG_INTERRUPTABLE,
  BS_FALL                  = 10,
  BS_SIT                   = 11 | BS_FLAG_FREEHANDS,
  BS_LIE                   = 12,
  BS_INVENTORY             = 13,
  BS_ITEMINTERACT          = 14 | BS_FLAG_INTERRUPTABLE,
  BS_MOBINTERACT           = 15,
  BS_MOBINTERACT_INTERRUPT = 16 | BS_FLAG_INTERRUPTABLE,
  BS_TAKEITEM              = 17,
  BS_DROPITEM              = 18,
  BS_THROWITEM             = 19,
  BS_PICKPOCKET            = 20 | BS_FLAG_INTERRUPTABLE,
  BS_STUMBLE               = 21,
  BS_UNCONSCIOUS           = 22,
  BS_DEAD                  = 23,
  BS_AIMNEAR               = 24,
  BS_AIMFAR                = 25,
  BS_HIT                   = 26 | BS_FLAG_INTERRUPTABLE,
  BS_PARADE                = 27,
  BS_CASTING               = 28,
  BS_PETRIFIED             = 29,
  BS_CONTROLLING           = 30 | BS_FLAG_INTERRUPTABLE,
  BS_MAX                   = 31,
  BS_STAND                 = BS_FLAG_INTERRUPTABLE | BS_FLAG_FREEHANDS,

  BS_MAX_FLAGS             = BS_MAX | BS_MOD_MASK | BS_FLAG_MASK,
  };

enum class WeaponState : uint8_t {
  NoWeapon,
  Fist,
  W1H,
  W2H,
  Bow,
  CBow,
  Mage
  };

enum class FightMode : int32_t {
  FMODE_NONE  = 0,
  FMODE_FIST  = 1,
  FMODE_MELEE = 2,
  FMODE_FAR   = 5,
  FMODE_MAGIC = 7,
  };

enum class WalkBit : uint8_t {
  WM_Run  =0,
  WM_Walk =1,
  WM_Sneak=2,
  WM_Water=4,
  WM_Swim =8,
  };

inline WalkBit operator & (WalkBit a,const WalkBit& b){
  return WalkBit(uint8_t(a)&uint8_t(b));
  }

inline WalkBit operator | (WalkBit a,const WalkBit& b){
  return WalkBit(uint8_t(a)|uint8_t(b));
  }

inline WalkBit operator ^ (WalkBit a,const WalkBit& b){
  return WalkBit(uint8_t(a)^uint8_t(b));
  }

enum Attitude : int32_t {
  ATT_HOSTILE  = 0,
  ATT_ANGRY    = 1,
  ATT_NEUTRAL  = 2,
  ATT_FRIENDLY = 3,
  ATT_NULL     = -1
  };

enum ItemMaterial : uint8_t {
  MAT_WOOD    = 0,
  MAT_STONE   = 1,
  MAT_METAL   = 2,
  MAT_LEATHER = 3,
  MAT_CLAY    = 4,
  MAT_GLAS    = 5
  };

enum CollideMask : uint32_t {
  COLL_DONOTHING         = 0,
  COLL_DOEVERYTHING      = 1,
  COLL_APPLYDAMAGE       = 2,
  COLL_APPLYHALVEDAMAGE  = 4,
  COLL_APPLYDOUBLEDAMAGE = 8,
  COLL_APPLYVICTIMSTATE  = 16,
  COLL_DONTKILL          = 32,
  };

enum SpellCategory:int32_t {
  SPELL_GOOD   =0,
  SPELL_NEUTRAL=1,
  SPELL_BAD    =2,
  };

enum class SpellFxKey:uint8_t {
  Open,
  Init,
  Cast,
  Invest,
  Collide,
  Count
  };

enum TargetCollect : int32_t {
  TARGET_COLLECT_NONE                  = 0, // target will be set by effect (range, azi, elev)
  TARGET_COLLECT_CASTER                = 1, // target is the caster
  TARGET_COLLECT_FOCUS                 = 2, // target is the focus vob
  TARGET_COLLECT_ALL                   = 3, // all targets in range will be assembled
  TARGET_COLLECT_FOCUS_FALLBACK_NONE   = 4, // target is the focus vob, if the focus vob is not valid, the trajectory will be set by the effect
  TARGET_COLLECT_FOCUS_FALLBACK_CASTER = 5, // target is the focus vob, if the focus vob is not valid, the target is the caster
  TARGET_COLLECT_ALL_FALLBACK_NONE     = 6, // all targets in range will be assembled, if there are no valid targets, the trajectory will be set by the effect
  TARGET_COLLECT_ALL_FALLBACK_CASTER   = 7, // all targets in range will be assembled, if there are no valid targets, the target is the caster
  };


enum class SensesBit : int32_t {
  SENSE_NONE  = 0,
  SENSE_SEE   = 1 << 0,
  SENSE_HEAR  = 1 << 1,
  SENSE_SMELL = 1 << 2,
  };

inline SensesBit operator & (SensesBit a,const SensesBit& b){
  return SensesBit(uint8_t(a)&uint8_t(b));
  }

inline SensesBit operator | (SensesBit a,const SensesBit& b){
  return SensesBit(uint8_t(a)|uint8_t(b));
  }

enum MenuItem : int32_t {
  MENU_ITEM_UNDEF     = 0,
  MENU_ITEM_TEXT      = 1,
  MENU_ITEM_SLIDER    = 2,
  MENU_ITEM_INPUT     = 3,
  MENU_ITEM_CURSOR    = 4,
  MENU_ITEM_CHOICEBOX = 5,
  MENU_ITEM_BUTTON    = 6,
  MENU_ITEM_LISTBOX   = 7,
  };
