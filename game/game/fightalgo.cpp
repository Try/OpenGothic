#include "fightalgo.h"

#include <Tempest/Log>

#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "gothic.h"
#include "serialize.h"

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

  if(ws==WeaponState::NoWeapon)
    return;

  if(hitFlg) {
    hitFlg = false;
    if(fillQueue(owner,ai.my_w_strafe))
      return;
    }

  const bool focus = isInFocusAngle(npc,tg);

  if(tg.isPrehit() && isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc)){
    if(tg.bodyStateMasked()==BS_RUN)
      if(fillQueue(owner,ai.enemy_stormprehit))
        return;
    if(fillQueue(owner,ai.enemy_prehit))
      return;
    }

  if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H) {
    if(isInWRange(npc,tg,owner)) {
      if(npc.bodyStateMasked()==BS_RUN && focus)
        if(fillQueue(owner,ai.my_w_runto))
          return;
      if(focus)
        if(fillQueue(owner,ai.my_w_focus))
          return;
      if(fillQueue(owner,ai.my_w_nofocus))
        return;
      }

    if(isInGRange(npc,tg,owner)) {
      if(npc.bodyStateMasked()==BS_RUN && focus)
        if(fillQueue(owner,ai.my_g_runto))
          return;
      if(focus)
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

bool FightAlgo::fillQueue(GameScript& owner,const phoenix::c_fight_ai &src) {
  uint32_t sz=0;
  for(size_t i=0;i<phoenix::c_fight_ai::move_count;++i){
    if(src.move[i]==phoenix::c_fight_ai_move::nop)
      break;
    sz++;
    }
  if(sz==0)
    return false;
  queueId = phoenix::c_fight_ai_move(src.move[owner.rand(sz)]);
  return queueId!=phoenix::c_fight_ai_move::nop;
  }

FightAlgo::Action FightAlgo::nextFromQueue(Npc& npc, Npc& tg, GameScript& owner) {
  using phoenix::c_fight_ai_move;

  if(tr[0]==MV_NULL) {
    switch(queueId) {
      case c_fight_ai_move::turn:
      case c_fight_ai_move::run:{
        if(isInGRange(npc,tg,owner))
          tr[0] = MV_MOVEA; else
          tr[0] = MV_MOVEG;
        break;
        }
      case c_fight_ai_move::run_back:{
        tr[0] = MV_NULL; //TODO
        break;
        }
      case c_fight_ai_move::jump_back:{
        tr[0] = MV_JUMPBACK;
        break;
        }
      case c_fight_ai_move::strafe:{
        tr[0] = owner.rand(2) ? MV_STRAFEL : MV_STRAFER;
        break;
        }
      case c_fight_ai_move::attack:{
        tr[0] = MV_ATACK;
        break;
        }
      case c_fight_ai_move::attack_side:{
        tr[0] = MV_ATACKL;
        tr[1] = MV_ATACKR;
        break;
        }
      case c_fight_ai_move::attack_front:{
        tr[0] = owner.rand(2) ? MV_ATACKL : MV_ATACKR;
        tr[1] = MV_ATACK;
        break;
        }
      case c_fight_ai_move::attack_triple:{
        if(owner.rand(2)){
          tr[0] = MV_ATACK;
          tr[1] = MV_ATACKR;
          tr[2] = MV_ATACKL;
          } else {
          tr[0] = MV_ATACKL;
          tr[1] = MV_ATACKR;
          tr[2] = MV_ATACK;
          }
        break;
        }
      case c_fight_ai_move::attack_whirl:{
        tr[0] = MV_ATACKL;
        tr[1] = MV_ATACKR;
        tr[2] = MV_ATACKL;
        tr[3] = MV_ATACKR;
        break;
        }
      case c_fight_ai_move::attack_master:{
        tr[0] = MV_ATACKL;
        tr[1] = MV_ATACKR;
        tr[2] = MV_ATACK;
        tr[3] = MV_ATACK;
        tr[4] = MV_ATACK;
        tr[5] = MV_ATACK;
        break;
        }
      case c_fight_ai_move::turn_to_hit:{
        tr[0] = MV_TURN2HIT;
        break;
        }
      case c_fight_ai_move::parry:{
        tr[0] = MV_BLOCK;
        break;
        }
      case c_fight_ai_move::stand_up:{
        break;
        }
      case c_fight_ai_move::wait:
      case c_fight_ai_move::wait_ext:{
        tr[0] = MV_WAIT;
        break;
        }
      case c_fight_ai_move::wait_longer:{
        tr[0] = MV_WAITLONG;
        break;
        }
      default: {
        static std::set<c_fight_ai_move> inst;
        if(inst.find(queueId)==inst.end()) {
          Tempest::Log::d("unrecognized FAI instruction: ", int(queueId));
          inst.insert(queueId);
          }
        }
      }
    queueId=phoenix::c_fight_ai_move::nop;
    }
  return tr[0];
  }

bool FightAlgo::hasInstructions() const {
  return tr[0]!=MV_NULL;
  }

bool FightAlgo::fetchInstructions(Npc &npc, Npc &tg, GameScript& owner) {
  fillQueue(npc,tg,owner);
  if(queueId==phoenix::c_fight_ai_move::nop)
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
  queueId = phoenix::c_fight_ai_move::nop;
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
  auto& gv     = owner.guildVal();
  float baseTg = float(gv.fight_range_base[tg .guild()]);
  auto  dist   = npc.qDistTo(tg);
  auto  pd     = baseTg + prefferedAttackDistance(npc,tg,owner);
  return (dist<=pd*pd);
  }

bool FightAlgo::isInWRange(const Npc& npc, const Npc& tg, GameScript& owner) const {
  auto dist = npc.qDistTo(tg);
  auto pd   = prefferedAttackDistance(npc,tg,owner);
  return (dist<=pd*pd);
  }

bool FightAlgo::isInGRange(const Npc &npc, const Npc &tg, GameScript &owner) const {
  auto dist = npc.qDistTo(tg);
  auto pd   = prefferedGDistance(npc,tg,owner);
  return (dist<=pd*pd);
  }

bool FightAlgo::isInFocusAngle(const Npc &npc, const Npc &tg) const {
  static const float maxAngle = std::cos(float(30.0*M_PI/180.0));

  const auto  dpos  = npc.position()-tg.position();
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

