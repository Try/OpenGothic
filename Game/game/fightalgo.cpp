#include "fightalgo.h"

#include "world/npc.h"
#include "world/item.h"

FightAlgo::FightAlgo() {
  }

void FightAlgo::fillQueue(Npc &npc, Npc &tg, WorldScript& owner) {
  auto& ai = owner.getFightAi(size_t(npc.handle()->fight_tactic));
  auto  ws = npc.weaponState();

  if(ws==WeaponState::NoWeapon)
    return;

  if(hitFlg){
    hitFlg = false;
    return fillQueue(owner,ai.my_w_strafe);
    }

  if(tg.isPrehit()){
    //return fillQueue(owner,ai.enemy_stormprehit);
    return fillQueue(owner,ai.enemy_prehit);
    }

  if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H){
    if(isInAtackRange(npc,tg,owner)) {
      if(npc.anim()==AnimationSolver::Move)
        return fillQueue(owner,ai.my_w_runto);
      if(isInAtackRange(npc,tg,owner))
        return fillQueue(owner,ai.my_w_focus);
      return fillQueue(owner,ai.my_w_nofocus);
      }

    if(isInGRange(npc,tg,owner)){
      if(npc.anim()==AnimationSolver::Move)
        return fillQueue(owner,ai.my_g_runto);
      return fillQueue(owner,ai.my_g_focus);
      }

    return fillQueue(owner,ai.my_w_nofocus);
    }

  if(ws==WeaponState::Bow || ws==WeaponState::CBow){
    if(isInAtackRange(npc,tg,owner))
      return fillQueue(owner,ai.my_fk_focus_far);
    return fillQueue(owner,ai.my_fk_nofocus_far);
    }

  if(ws==WeaponState::Mage){
    if(isInAtackRange(npc,tg,owner))
      return fillQueue(owner,ai.my_fk_focus_mag);
    return fillQueue(owner,ai.my_fk_nofocus_mag);
    }

  return fillQueue(owner,ai.my_w_nofocus);
  }

void FightAlgo::fillQueue(WorldScript& owner,const Daedalus::GEngineClasses::C_FightAI &src) {
  size_t sz=0;
  for(size_t i=0;i<Daedalus::GEngineClasses::MAX_MOVE;++i){
    if(src.move[i]==0)
      break;
    sz++;
    }
  if(sz==0)
    return;
  queueId = src.move[owner.rand(sz)];
  }

FightAlgo::Action FightAlgo::nextFromQueue(WorldScript& owner) {
  if(tr[0]==MV_NULL){
    switch(queueId) {
      case Daedalus::GEngineClasses::MOVE_RUN:{
        tr[0] = MV_MOVE;
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
      case Daedalus::GEngineClasses::MOVE_TURN:{
        tr[0] = MV_MOVE;
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
        // nop
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
        waitT = 200;
        break;
        }
      case Daedalus::GEngineClasses::MOVE_WAIT_LONGER:{
        waitT = 300;
        break;
        }
      case Daedalus::GEngineClasses::MAX_FIGHTAI:
        break;
      }
    queueId=Daedalus::GEngineClasses::Move(0);
    }
  return tr[0];
  }

FightAlgo::Action FightAlgo::tick(Npc &npc, Npc &tg, WorldScript& owner, uint64_t dt) {
  if(uint64_t(waitT)>=dt){
    waitT-=dt;
    return MV_NULL;
    }
  waitT=0;

  if(queueId==0){
    fillQueue(npc,tg,owner);
    }
  return nextFromQueue(owner);
  }

void FightAlgo::consumeAction() {
  if(tr[0]==MV_STRAFEL || tr[0]==MV_STRAFER)
    waitT = 300;

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

float FightAlgo::prefferedAtackDistance(const Npc &npc, const Npc &tg,  WorldScript &owner) const {
  auto  gl     = std::min<uint32_t>(tg.guild(), GIL_MAX);
  float baseTg = owner.guildVal().fight_range_base[gl];

  return baseTg+weaponRange(owner,npc);
  }

bool FightAlgo::isInAtackRange(const Npc &npc,const Npc &tg, WorldScript &owner) {
  auto dist = npc.qDistTo(tg);
  auto pd   = prefferedAtackDistance(npc,tg,owner);
  return (dist<pd*pd);
  }

bool FightAlgo::isInGRange(const Npc &npc, const Npc &tg, WorldScript &owner) {
  auto dist = npc.qDistTo(tg);
  auto pd   = gRange(owner,npc);
  return (dist<pd*pd);
  }

float FightAlgo::gRange(WorldScript &owner, const Npc &npc) {
  auto  gl = std::min<uint32_t>(npc.guild(),GIL_MAX);
  auto& gv = owner.guildVal();
  return gv.fight_range_g[gl]+gv.fight_range_base[gl]+weaponOnlyRange(owner,npc);
  }

float FightAlgo::weaponRange(WorldScript &owner, const Npc &npc) {
  auto  gl = std::min<uint32_t>(npc.guild(),GIL_MAX);
  auto& gv = owner.guildVal();
  return gv.fight_range_base[gl]+weaponOnlyRange(owner,npc);
  }

float FightAlgo::weaponOnlyRange(WorldScript &owner,const Npc &npc) {
  auto  gl  = std::min<uint32_t>(npc.guild(),GIL_MAX);
  auto& gv  = owner.guildVal();
  auto  w   = npc.inventory().activeWeapon();
  int   add = w ? w->swordLength() : 0;

  switch(npc.weaponState()) {
    case WeaponState::W1H:
      return gv.fight_range_1ha[gl] + add;
    case WeaponState::W2H:
      return gv.fight_range_2ha[gl] + add;
    case WeaponState::NoWeapon:
    case WeaponState::Fist:
      return gv.fight_range_fist[gl];
    case WeaponState::Bow:
    case WeaponState::CBow:
    case WeaponState::Mage:
      return 800;
    }
  return 0;
  }
