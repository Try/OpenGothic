#include "fightalgo.h"

#include <Tempest/Log>

#include "world/npc.h"
#include "world/item.h"
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
  auto& ai = owner.getFightAi(size_t(npc.handle()->fight_tactic));
  auto  ws = npc.weaponState();

  if(ws==WeaponState::NoWeapon)
    return;

  if(hitFlg){
    hitFlg = false;
    if(fillQueue(owner,ai.my_w_strafe))
      return;
    }

  if(tg.isPrehit() && isInGRange(npc,tg,owner)){
    if(tg.bodyStateMasked()==BS_RUN)
      if(fillQueue(owner,ai.enemy_stormprehit))
        return;
    if(fillQueue(owner,ai.enemy_prehit))
      return;
    }

  if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H){
    if(isInAtackRange(npc,tg,owner)) {
      if(npc.bodyStateMasked()==BS_RUN)
        if(fillQueue(owner,ai.my_w_runto))
          return;
      if(isInFocusAngle(npc,tg))
        if(fillQueue(owner,ai.my_w_focus))
          return;
      if(fillQueue(owner,ai.my_w_nofocus))
        return;
      }

    if(isInGRange(npc,tg,owner)) {
      if(npc.bodyStateMasked()==BS_RUN)
        if(fillQueue(owner,ai.my_g_runto))
          return;
      if(isInFocusAngle(npc,tg))
        if(fillQueue(owner,ai.my_g_focus))
          return;
      }

    if(npc.bodyStateMasked()==BS_RUN)
      if(fillQueue(owner,ai.my_w_runto))
        return;
    if(fillQueue(owner,ai.my_w_nofocus))
      return;
    }

  if(ws==WeaponState::Bow || ws==WeaponState::CBow){
    if(isInAtackRange(npc,tg,owner))
      if(fillQueue(owner,ai.my_fk_focus_far))
        return;
    if(fillQueue(owner,ai.my_fk_nofocus_far))
      return;
    }

  if(ws==WeaponState::Mage){
    if(isInAtackRange(npc,tg,owner))
      if(fillQueue(owner,ai.my_fk_focus_mag))
        return;
    if(fillQueue(owner,ai.my_fk_nofocus_mag))
      return;
    }

  fillQueue(owner,ai.my_w_nofocus);
  }

bool FightAlgo::fillQueue(GameScript& owner,const Daedalus::GEngineClasses::C_FightAI &src) {
  uint32_t sz=0;
  for(size_t i=0;i<Daedalus::GEngineClasses::MAX_MOVE;++i){
    if(src.move[i]==0)
      break;
    sz++;
    }
  if(sz==0)
    return false;
  queueId = src.move[owner.rand(sz)];
  return queueId!=0;
  }

FightAlgo::Action FightAlgo::nextFromQueue(Npc& npc, Npc& tg, GameScript& owner) {
  if(tr[0]==MV_NULL){
    switch(queueId) {
      case Daedalus::GEngineClasses::MOVE_TURN:
      case Daedalus::GEngineClasses::MOVE_RUN:{
        if(isInGRange(npc,tg,owner))
          tr[0] = MV_MOVEA; else
          tr[0] = MV_MOVEG;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_RUNBACK:{
        tr[0] = MV_NULL; //TODO
        break;
        }
      case Daedalus::GEngineClasses::MOVE_JUMPBACK:{
        tr[0] = MV_JUMPBACK;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_STRAFE:{
        tr[0] = owner.rand(2) ? MV_STRAFEL : MV_STRAFER;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_ATTACK:{
        tr[0] = MV_ATACK;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_SIDEATTACK:{
        tr[0] = MV_ATACKL;
        tr[1] = MV_ATACKR;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_FRONTATTACK:{
        tr[0] = owner.rand(2) ? MV_ATACKL : MV_ATACKR;
        tr[1] = MV_ATACK;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_TRIPLEATTACK:{
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
      case Daedalus::GEngineClasses::MOVE_WHIRLATTACK:{
        tr[0] = MV_ATACKL;
        tr[1] = MV_ATACKR;
        tr[2] = MV_ATACKL;
        tr[3] = MV_ATACKR;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_MASTERATTACK:{
        tr[0] = MV_ATACKL;
        tr[1] = MV_ATACKR;
        tr[2] = MV_ATACK;
        tr[3] = MV_ATACK;
        tr[4] = MV_ATACK;
        tr[5] = MV_ATACK;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_TURNTOHIT:{
        tr[0] = MV_TURN2HIT;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_PARADE:{
        tr[0] = MV_BLOCK;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_STANDUP:{
        break;
        }
      case Daedalus::GEngineClasses::MOVE_WAIT:
      case Daedalus::GEngineClasses::MOVE_WAIT_EXT:{
        tr[0] = MV_WAIT;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_WAIT_LONGER:{
        tr[0] = MV_WAITLONG;
        break;
        }
      default: {
        static std::unordered_set<int32_t> inst;
        if(inst.find(queueId)==inst.end()) {
          Tempest::Log::d("unrecognized FAI instruction: ",queueId);
          inst.insert(queueId);
          }
        }
      }
    queueId=Daedalus::GEngineClasses::Move(0);
    }
  return tr[0];
  }

bool FightAlgo::hasInstructions() const {
  return tr[0]!=MV_NULL;
  }

bool FightAlgo::fetchInstructions(Npc &npc, Npc &tg, GameScript& owner) {
  fillQueue(npc,tg,owner);
  nextFromQueue(npc,tg,owner);
  return true;
  }

void FightAlgo::consumeAction() {
  for(size_t i=1;i<MV_MAX;++i)
    tr[i-1]=tr[i];
  tr[MV_MAX-1]=MV_NULL;
  }

void FightAlgo::onClearTarget() {
  queueId = Daedalus::GEngineClasses::Move(0);
  tr[0]   = MV_NULL;
  }

void FightAlgo::onTakeHit() {
  hitFlg = true;
  for(auto& i:tr)
    i = MV_NULL;
  }

float FightAlgo::prefferedAtackDistance(const Npc &npc, const Npc &tg,  GameScript &owner) const {
  auto  gl     = tg.guild();
  float baseTg = float(owner.guildVal().fight_range_base[gl]);

  return baseTg+weaponRange(owner,npc);
  }

float FightAlgo::prefferedGDistance(const Npc &npc, const Npc &tg, GameScript &owner) const {
  auto  gl     = tg.guild();
  float baseTg = float(owner.guildVal().fight_range_base[gl]);

  return baseTg+gRange(owner,npc);
  }

bool FightAlgo::isInAtackRange(const Npc &npc,const Npc &tg, GameScript &owner) const {
  auto dist = npc.qDistTo(tg);
  auto pd   = prefferedAtackDistance(npc,tg,owner);
  return (dist<pd*pd);
  }

bool FightAlgo::isInGRange(const Npc &npc, const Npc &tg, GameScript &owner) const {
  auto dist = npc.qDistTo(tg);
  auto pd   = prefferedGDistance(npc,tg,owner);
  return (dist<pd*pd);
  }

bool FightAlgo::isInFocusAngle(const Npc &npc, const Npc &tg) const {
  static const float maxAngle = std::cos(float(M_PI/12));

  const float dx    = npc.position()[0]-tg.position()[0];
  const float dz    = npc.position()[2]-tg.position()[2];
  const float plAng = npc.rotationRad()+float(M_PI/2);

  const float da = plAng-std::atan2(dz,dx);
  const float c  = std::cos(da);

  if(c<maxAngle)
    return false;
  return true;
  }

float FightAlgo::gRange(GameScript &owner, const Npc &npc) {
  auto  gl = npc.guild();
  auto& gv = owner.guildVal();
  return float(gv.fight_range_g[gl]+gv.fight_range_base[gl])+weaponOnlyRange(owner,npc);
  }

float FightAlgo::weaponRange(GameScript &owner, const Npc &npc) {
  auto  gl = npc.guild();
  auto& gv = owner.guildVal();
  return float(gv.fight_range_base[gl])+weaponOnlyRange(owner,npc);
  }

float FightAlgo::weaponOnlyRange(GameScript &owner,const Npc &npc) {
  auto  gl  = npc.guild();
  auto& gv  = owner.guildVal();
  auto  w   = npc.inventory().activeWeapon();
  int   add = w ? w->swordLength() : 0;

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
    case WeaponState::Mage:
      return 800;
    }
  return 0;
  }

