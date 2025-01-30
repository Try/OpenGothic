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
  GIL_EMPTY_Z                 = 65,
  GIL_MAX                     = 66,

  // Gothic 1 guilds
  GIL_G1_NONE                 = GIL_NONE,
  GIL_G1_HUMAN                = GIL_HUMAN,
  GIL_G1_EBR                  = 1,  // Erzbaron
  GIL_G1_GRD                  = 2,  // Gardist
  GIL_G1_STT                  = 3,  // Schatten
  GIL_G1_KDF                  = GIL_KDF,
  GIL_G1_VLK                  = 5,  // Buddler
  GIL_G1_KDW                  = 6,  // Wassermagier
  GIL_G1_SLD                  = GIL_SLD,
  GIL_G1_ORG                  = 8,  // Bandit
  GIL_G1_BAU                  = 9,  // Bauer
  GIL_G1_SFB                  = 10, // Schuerfer
  GIL_G1_GUR                  = 11, // Guru
  GIL_G1_NOV                  = 12, // Novize
  GIL_G1_TPL                  = 13, // Templer
  GIL_G1_DMB                  = 14, // Darkmagic Xardas
  GIL_G1_BAB                  = 15, // Female
  GIL_G1_SEPERATOR_HUM        = GIL_SEPERATOR_HUM,
  GIL_G1_WARAN                = 17,
  GIL_G1_SLF                  = 18, // Sleeper
  GIL_G1_GOBBO                = GIL_GOBBO,
  GIL_G1_TROLL                = 20,
  GIL_G1_SNAPPER              = 21,
  GIL_G1_MINECRAWLER          = 22,
  GIL_G1_MEATBUG              = 23,
  GIL_G1_SCAVENGER            = 24,
  GIL_G1_DEMON                = 25,
  GIL_G1_WOLF                 = 26,
  GIL_G1_SHADOWBEAST          = 27,
  GIL_G1_BLOODFLY             = 28,
  GIL_G1_SWAMPSHARK           = 29,
  GIL_G1_ZOMBIE               = 30,
  GIL_G1_UNDEADORC            = 31,
  GIL_G1_SKELETON             = 32,
  GIL_G1_ORCDOG               = 33,
  GIL_G1_MOLERAT              = 34,
  GIL_G1_GOLEM                = 35,
  GIL_G1_LURKER               = 36,
  GIL_G1_SEPERATOR_ORC        = 37,
  GIL_G1_ORCSHAMAN            = 38,
  GIL_G1_ORCWARROIR           = 39,
  GIL_G1_ORCSCOUT             = 40,
  GIL_G1_ORCSLAVE             = 41,
  GIL_G1_MAX                  = 42,
  };

enum {
  MAX_AI_USE_DISTANCE=150
  };

enum {
  ReferenceBowRangeG1 = 2000,
  ReferenceBowRangeG2 = 1500,
  MaxBowRange         = 4500,
  MaxMagRange         = 3500, // from Focus_Ranged
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
  WM_Dive =16,
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

inline WalkBit operator ~ (WalkBit a){
  return WalkBit(~uint8_t(a));
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
  MAT_GLAS    = 5,
  MAT_COUNT   = 6,
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

enum TargetType : int32_t {
  TARGET_TYPE_ALL     = 1,
  TARGET_TYPE_ITEMS   = 2,
  TARGET_TYPE_NPCS    = 4,
  TARGET_TYPE_ORCS    = 8,
  TARGET_TYPE_HUMANS  = 16,
  TARGET_TYPE_UNDEAD  = 32,
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

enum ItmFlags : uint32_t {
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
  ITM_TORCH      = 1 << 28
  };

enum Action:uint32_t {
  AI_None  =0,
  AI_LookAtNpc,
  AI_StopLookAt,
  AI_RemoveWeapon,
  AI_TurnToNpc,
  AI_GoToNpc,
  AI_GoToNextFp,
  AI_GoToPoint,
  AI_StartState,
  AI_PlayAnim,
  AI_PlayAnimBs,
  AI_Wait,
  AI_StandUp,
  AI_StandUpQuick,
  AI_EquipArmor,
  AI_EquipBestArmor,
  AI_EquipMelee,
  AI_EquipRange,
  AI_UseMob,
  AI_UseItem,
  AI_UseItemToState,
  AI_Teleport,
  AI_DrawWeaponMelee,
  AI_DrawWeaponRange,
  AI_DrawSpell,
  AI_Attack,
  AI_Flee,
  AI_Dodge,
  AI_UnEquipWeapons,
  AI_UnEquipArmor,
  AI_Output,
  AI_OutputSvm,
  AI_OutputSvmOverlay,
  AI_ProcessInfo,
  AI_StopProcessInfo,
  AI_ContinueRoutine,
  AI_AlignToFp,
  AI_AlignToWp,
  AI_SetNpcsToState,
  AI_SetWalkMode,
  AI_FinishingMove,
  AI_DrawWeapon,
  AI_TakeItem,
  AI_GotoItem,
  AI_PointAtNpc,
  AI_PointAt,
  AI_StopPointAt,
  AI_PrintScreen,
  AI_LookAt,
  AI_WhirlToNpc,
  AI_TurnAway,
  };


enum PercType : uint8_t {
  // active perceptions
  PERC_ASSESSPLAYER       = 1,
  PERC_ASSESSENEMY        = 2,
  PERC_ASSESSFIGHTER      = 3,
  PERC_ASSESSBODY         = 4,
  PERC_ASSESSITEM         = 5,
  // passive perceptions
  PERC_ASSESSMURDER       = 6,
  PERC_ASSESSDEFEAT       = 7,
  PERC_ASSESSDAMAGE       = 8,
  PERC_ASSESSOTHERSDAMAGE = 9,
  PERC_ASSESSTHREAT       = 10,
  PERC_ASSESSREMOVEWEAPON = 11,
  PERC_OBSERVEINTRUDER    = 12,
  PERC_ASSESSFIGHTSOUND   = 13,
  PERC_ASSESSQUIETSOUND   = 14,
  PERC_ASSESSWARN         = 15,
  PERC_CATCHTHIEF         = 16,
  PERC_ASSESSTHEFT        = 17,
  PERC_ASSESSCALL         = 18,
  PERC_ASSESSTALK         = 19,
  PERC_ASSESSGIVENITEM    = 20,
  PERC_ASSESSFAKEGUILD    = 21,
  PERC_MOVEMOB            = 22,
  PERC_MOVENPC            = 23,
  PERC_DRAWWEAPON         = 24,
  PERC_OBSERVESUSPECT     = 25,
  PERC_NPCCOMMAND         = 26,
  PERC_ASSESSMAGIC        = 27,
  PERC_ASSESSSTOPMAGIC    = 28,
  PERC_ASSESSCASTER       = 29,
  PERC_ASSESSSURPRISE     = 30,
  PERC_ASSESSENTERROOM    = 31,
  PERC_ASSESSUSEMOB       = 32,
  PERC_Count
  };

enum Talent : uint8_t {
  TALENT_UNKNOWN            = 0,
  TALENT_1H                 = 1,
  TALENT_2H                 = 2,
  TALENT_BOW                = 3,
  TALENT_CROSSBOW           = 4,
  TALENT_PICKLOCK           = 5,
  TALENT_MAGE               = 7,
  TALENT_SNEAK              = 8,
  TALENT_REGENERATE         = 9,
  TALENT_FIREMASTER         = 10,
  TALENT_ACROBAT            = 11,
  TALENT_PICKPOCKET         = 12,
  TALENT_SMITH              = 13,
  TALENT_RUNES              = 14,
  TALENT_ALCHEMY            = 15,
  TALENT_TAKEANIMALTROPHY   = 16,
  TALENT_FOREIGNLANGUAGE    = 17,
  TALENT_WISPDETECTOR       = 18,
  TALENT_C                  = 19,
  TALENT_D                  = 20,
  TALENT_E                  = 21,

  TALENT_MAX_G1             = 12,
  TALENT_MAX_G2             = 22
  };

enum Attribute : uint8_t {
  ATR_HITPOINTS      = 0,
  ATR_HITPOINTSMAX   = 1,
  ATR_MANA           = 2,
  ATR_MANAMAX        = 3,
  ATR_STRENGTH       = 4,
  ATR_DEXTERITY      = 5,
  ATR_REGENERATEHP   = 6,
  ATR_REGENERATEMANA = 7,
  ATR_MAX
  };

enum SpellCode : int32_t {
  SPL_DONTINVEST                  = 0,
  SPL_RECEIVEINVEST               = 1,
  SPL_SENDCAST                    = 2,
  SPL_SENDSTOP                    = 3,
  SPL_NEXTLEVEL                   = 4,
  SPL_STATUS_CANINVEST_NO_MANADEC = 8,
  SPL_FORCEINVEST                 = 1 << 16
  };

enum Protection : uint8_t {
  PROT_BARRIER = 0,
  PROT_BLUNT   = 1,
  PROT_EDGE    = 2,
  PROT_FIRE    = 3,
  PROT_FLY     = 4,
  PROT_MAGIC   = 5,
  PROT_POINT   = 6,
  PROT_FALL    = 7,
  PROT_MAX     = 8
  };

inline const char* MaterialGroupNames[] = {
  "UNDEF",
  "METAL",
  "STONE",
  "WOOD",
  "EARTH",
  "WATER",
  "SNOW",
  };

enum AiStateCode : int32_t {
  LOOP_CONTINUE = 0,
  LOOP_END      = 1,
  };

enum class ScriptLang : int32_t {
  NONE = -1,
  EN   = 0,
  DE   = 1,
  PL   = 2,
  RU   = 3,
  FR   = 4,
  ES   = 5,
  IT   = 6,
  CZ   = 7,
  };

enum class AaPreset : uint32_t {
  OFF,
  MEDIUM,
  ULTRA,
  PRESETS_COUNT
  };

enum TurnAnim : uint8_t {
  TURN_ANIM_STD = 0,
  TURN_ANIM_NONE,
  TURN_ANIM_WHIRL,
};
