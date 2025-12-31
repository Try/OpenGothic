#include "fightalgo.h"

#include <Tempest/Log>

#include "game/definitions/fightaidefinitions.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "gothic.h"
#include "serialize.h"

// According to Gothic1 scripts:
// W  - Weapon Range (FIGHT_RANGE_FIST * 3)
// G  - Walking range (3 * W). Buffer for ranged fighters in which they should switch to a melee weapon.
// FK - Long-range combat range (30m)

FightAlgo::FightAlgo() {
  }

void FightAlgo::load(Serialize &fin) {
  fin.read(reinterpret_cast<int32_t&>(queueId));
  for(int i=0;i<MV_MAX;++i)
    fin.read(reinterpret_cast<uint8_t&>(tr[i]));
  fin.read(hitFlg);
  }

void FightAlgo::save(Serialize &fout) {
  fout.write(int32_t(queueId));
  for(int i=0;i<MV_MAX;++i)
    fout.write(uint8_t(tr[i]));
  fout.write(hitFlg);
  }

void FightAlgo::fillQueue(Npc &npc, Npc &tg, GameScript& owner) {
  auto& ai = Gothic::fai()[size_t(npc.handle().fight_tactic)];
  auto  ws = npc.weaponState();

  if(hitFlg) {
    hitFlg = false;
    if(fillQueue(owner,ai.my_w_strafe))
      return;
    }

  const bool focus = isInFocusAngle(npc,tg);

  if(tg.isPrehit() && isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc) && focus){
    if(tg.bodyStateMasked()==BS_RUN)
      if(fillQueue(owner,ai.enemy_stormprehit))
        return;
    if(fillQueue(owner,ai.enemy_prehit))
      return;
    }

  if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H) {
    if(isInWRange(npc,tg,owner)) {
      if(focus && npc.bodyStateMasked()==BS_RUN)
        if(fillQueue(owner,ai.my_w_runto))
          return;
      if(focus && npc.bodyStateMasked()!=BS_RUN)
        if(fillQueue(owner,ai.my_w_focus))
          return;
      if(fillQueue(owner,ai.my_w_nofocus))
        return;
      }

    if(isInGRange(npc,tg,owner)) {
      if(focus && npc.bodyStateMasked()==BS_RUN)
        if(fillQueue(owner,ai.my_g_runto))
          return;
      if(focus && npc.bodyStateMasked()!=BS_RUN)
        if(fillQueue(owner,ai.my_g_focus))
          return;
      }
    }

  if(ws==WeaponState::Mage) {
    if(isInWRange(npc,tg,owner))
      if(fillQueue(owner,ai.my_fk_focus_mag))
        return;
    if(fillQueue(owner,ai.my_fk_nofocus_mag))
      return;
    }

  if(isInWRange(npc,tg,owner) && focus)
    if(fillQueue(owner,ai.my_fk_focus_far))
      return;
  if(fillQueue(owner,ai.my_fk_nofocus_far))
    return;

  fillQueue(owner,ai.my_w_nofocus);
  }

bool FightAlgo::fillQueue(GameScript& owner, const zenkit::IFightAi& src) {
  uint32_t sz=0;
  for(size_t i=0; i<zenkit::IFightAi::move_count; ++i){
    if(src.move[i]==zenkit::FightAiMove::NOP)
      break;
    sz++;
    }
  if(sz==0)
    return false;
  queueId = zenkit::FightAiMove(src.move[owner.rand(sz)]);
  return queueId!=zenkit::FightAiMove::NOP;
  }

FightAlgo::Action FightAlgo::nextFromQueue(Npc& npc, Npc& tg, GameScript& owner) {
  using zenkit::FightAiMove;

  if(tr[0]==MV_NULL) {
    switch(queueId) {
      case FightAiMove::TURN:
        if(!isInGRange(npc,tg,owner))
          tr[0] = MV_TURNG; else
          tr[0] = MV_TURNA;
        break;
      case FightAiMove::RUN:{
        if(!isInGRange(npc,tg,owner))
          tr[0] = MV_MOVEG; else
          tr[0] = MV_MOVEA;
        break;
        }
      case FightAiMove::RUN_BACK:{
        tr[0] = MV_NULL; //TODO
        break;
        }
      case FightAiMove::JUMP_BACK:{
        tr[0] = MV_JUMPBACK;
        break;
        }
      case FightAiMove::STRAFE:{
        tr[0] = owner.rand(2) ? MV_STRAFEL : MV_STRAFER;
        break;
        }
      case FightAiMove::ATTACK:{
        tr[0] = MV_ATTACK;
        break;
        }
      case FightAiMove::ATTACK_SIDE:{
        tr[0] = MV_ATTACKL;
        tr[1] = MV_ATTACKR;
        break;
        }
      case FightAiMove::ATTACK_FRONT:{
        tr[0] = owner.rand(2) ? MV_ATTACKL : MV_ATTACKR;
        tr[1] = MV_ATTACK;
        break;
        }
      case FightAiMove::ATTACK_TRIPLE:{
        if(owner.rand(2)){
          tr[0] = MV_ATTACK;
          tr[1] = MV_ATTACKR;
          tr[2] = MV_ATTACKL;
          } else {
          tr[0] = MV_ATTACKL;
          tr[1] = MV_ATTACKR;
          tr[2] = MV_ATTACK;
          }
        break;
        }
      case FightAiMove::ATTACK_WHIRL:{
        tr[0] = MV_ATTACKL;
        tr[1] = MV_ATTACKR;
        tr[2] = MV_ATTACKL;
        tr[3] = MV_ATTACKR;
        break;
        }
      case FightAiMove::ATTACK_MASTER:{
        tr[0] = MV_ATTACKL;
        tr[1] = MV_ATTACKR;
        tr[2] = MV_ATTACK;
        tr[3] = MV_ATTACK;
        tr[4] = MV_ATTACK;
        tr[5] = MV_ATTACK;
        break;
        }
      case FightAiMove::TURN_TO_HIT:{
        tr[0] = MV_TURN2HIT;
        break;
        }
      case FightAiMove::PARRY:{
        tr[0] = MV_BLOCK;
        break;
        }
      case FightAiMove::STAND_UP:{
        break;
        }
      case FightAiMove::WAIT:
      case FightAiMove::WAIT_EXT:{
        tr[0] = MV_WAIT;
        break;
        }
      case FightAiMove::WAIT_LONGER:{
        tr[0] = MV_WAITLONG;
        break;
        }
      default: {
        static std::set<FightAiMove> inst;
        if(inst.find(queueId)==inst.end()) {
          Tempest::Log::d("unrecognized FAI instruction: ", int(queueId));
          inst.insert(queueId);
          }
        }
      }
    queueId = FightAiMove::NOP;
    }
  return tr[0];
  }

bool FightAlgo::hasInstructions() const {
  return tr[0]!=MV_NULL;
  }

bool FightAlgo::fetchInstructions(Npc &npc, Npc &tg, GameScript& owner) {
  fillQueue(npc,tg,owner);
  if(queueId==zenkit::FightAiMove::NOP)
    return false;
  nextFromQueue(npc,tg,owner);
  return true;
  }

void FightAlgo::consumeAction() {
  for(size_t i=1;i<MV_MAX;++i)
    tr[i-1]=tr[i];
  tr[MV_MAX-1]=MV_NULL;
  }

void FightAlgo::onClearTarget() {
  queueId = zenkit::FightAiMove::NOP;
  tr[0]   = MV_NULL;
  }

void FightAlgo::onTakeHit() {
  hitFlg = true;
  for(auto& i:tr)
    i = MV_NULL;
  }

float FightAlgo::baseDistance(const Npc& npc, const Npc& tg,  GameScript &owner) const {
  auto&  gv      = owner.guildVal();
  float  baseTg  = float(gv.fight_range_base[tg .guild()]);
  float  baseNpc = float(gv.fight_range_base[npc.guild()]);
  return baseTg + baseNpc;
  }

float FightAlgo::prefferedAttackDistance(const Npc& npc, const Npc& tg,  GameScript &owner) const {
  auto&  gv      = owner.guildVal();
  float  baseTg  = float(gv.fight_range_base[tg .guild()]);
  float  baseNpc = float(gv.fight_range_base[npc.guild()]);
  return baseTg + baseNpc + weaponRange(owner,npc);
  }

float FightAlgo::prefferedGDistance(const Npc& npc, const Npc& tg, GameScript &owner) const {
  auto   gl      = npc.guild();
  auto&  gv      = owner.guildVal();
  float  baseTg  = float(gv.fight_range_base[tg .guild()]);
  float  baseNpc = float(gv.fight_range_base[npc.guild()]);
  return float(baseTg + baseNpc + float(gv.fight_range_g[gl])) + weaponRange(owner,npc);
  }

bool FightAlgo::isInAttackRange(const Npc &npc,const Npc &tg, GameScript &owner) const {
  // tested in vanilla on Bloofly's:
  //  60 weapon range (Spiked club) is not enough to hit
  //  70 weapon range (Rusty Sword) is good to hit
  auto  dist   = npc.qDistTo(tg);
  auto  pd     = prefferedAttackDistance(npc,tg,owner);
  static float padding = 0;
  if(npc.hasState(BS_RUN))
    pd += padding; // padding, for wolf
  return (dist<=pd*pd);
  }

bool FightAlgo::isInWRange(const Npc& npc, const Npc& tg, GameScript& owner) const {
  auto dist = npc.qDistTo(tg);
  auto pd   = prefferedAttackDistance(npc,tg,owner);
  return (dist<=pd*pd);
  }

bool FightAlgo::isInGRange(const Npc &npc, const Npc &tg, GameScript &owner) const {
  auto  dist    = npc.qDistTo(tg);
  auto  pd      = prefferedGDistance(npc,tg,owner);
  return (dist<=pd*pd);
  }

bool FightAlgo::isInFocusAngle(const Npc &npc, const Npc &tg) const {
  static const float maxAngle = std::cos(float(30.0*M_PI/180.0));

  const auto  dpos  = npc.centerPosition()-tg.centerPosition();
  const float plAng = npc.rotationRad()+float(M_PI/2);

  const float da = plAng-std::atan2(dpos.z,dpos.x);
  const float c  = std::cos(da);

  if(c<maxAngle)
    return false;
  return true;
  }

float FightAlgo::weaponRange(GameScript &owner, const Npc &npc) {
  auto  gl  = npc.guild();
  auto& gv  = owner.guildVal();
  auto  w   = npc.inventory().activeWeapon();
  int   add = w ? w->swordLength() : 0;
  auto  bR  = Gothic::inst().version().game==2 ? ReferenceBowRangeG2 : ReferenceBowRangeG1;

  switch(npc.weaponState()) {
    case WeaponState::W1H:
      return float(gv.fight_range_1ha[gl] + add);
    case WeaponState::W2H:
      return float(gv.fight_range_2ha[gl] + add);
    case WeaponState::NoWeapon:
    case WeaponState::Fist:
      return float(gv.fight_range_fist[gl]);
    case WeaponState::Bow:
    case WeaponState::CBow:
      return float(bR);
    case WeaponState::Mage:
      return float(MaxMagRange);
    }
  return 0;
  }

