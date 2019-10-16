#include "animationsolver.h"

#include "skeleton.h"
#include "pose.h"
#include "world/interactive.h"
#include "world/world.h"
#include "game/serialize.h"

#include "resources.h"

using namespace Tempest;

static const std::pair<const char*,AnimationSolver::Anim> schemes[]={
  {"FOOD",       AnimationSolver::Anim::Food1},
  {"FOODHUGE",   AnimationSolver::Anim::FoodHuge1},
  {"POTION",     AnimationSolver::Anim::Potition1},
  {"POTIONFAST", AnimationSolver::Anim::PotitionFast},
  {"JOINT",      AnimationSolver::Anim::Joint1},
  {"MAP",        AnimationSolver::Anim::Map1},
  {"MAPSEALED",  AnimationSolver::Anim::MapSeal1},
  {"FIRESPIT",   AnimationSolver::Anim::Firespit1},
  {"MEAT",       AnimationSolver::Anim::Meat1},
  {"RICE",       AnimationSolver::Anim::Rice1},
  };

AnimationSolver::AnimationSolver() {
  }

void AnimationSolver::save(Serialize &fout) {
  fout.write(uint32_t(overlay.size()));
  for(auto& i:overlay){
    fout.write(i.skeleton->name(),i.time);
    }
  }

void AnimationSolver::load(Serialize &fin) {
  uint32_t sz=0;
  std::string s;

  fin.read(sz);
  overlay.resize(sz);
  for(auto& i:overlay){
    fin.read(s,i.time);
    i.skeleton = Resources::loadSkeleton(s);
    }

  sz=0;
  for(size_t i=0;i<overlay.size();++i){
    overlay[sz]=overlay[i];
    if(overlay[i].skeleton!=nullptr)
      ++sz;
    }
  overlay.resize(sz);
  }

void AnimationSolver::setSkeleton(const Skeleton *sk) {
  baseSk = sk;
  }

void AnimationSolver::addOverlay(const Skeleton* sk,uint64_t time) {
  if(sk==nullptr)
    return;
  Overlay ov = {sk,time};
  overlay.push_back(ov);
  }

void AnimationSolver::delOverlay(const char *sk) {
  if(overlay.size()==0)
    return;
  auto skelet = Resources::loadSkeleton(sk);
  delOverlay(skelet);
  }

void AnimationSolver::delOverlay(const Skeleton *sk) {
  for(size_t i=0;i<overlay.size();++i)
    if(overlay[i].skeleton==sk){
      overlay.erase(overlay.begin()+int(i));
      return;
      }
  }

void AnimationSolver::update(uint64_t tickCount) {
  for(size_t i=0;i<overlay.size();){
    auto& ov = overlay[i];
    if(ov.time!=0 && ov.time<tickCount)
      overlay.erase(overlay.begin()+int(i)); else
      ++i;
    }
  }

const Animation::Sequence* AnimationSolver::solveAnim(AnimationSolver::Anim a, WeaponState st, WalkBit wlkMode, const Pose& pose) const {
  // Atack
  if(st==WeaponState::Fist) {
    if(a==Anim::Atack && pose.isInAnim("S_FISTRUNL")) {
      if(auto s=solveFrm("T_FISTATTACKMOVE"))
        return s;
      }
    if(a==Anim::Atack)
      return solveFrm("S_FISTATTACK");
    if(a==Anim::AtackBlock)
      return solveFrm("T_FISTPARADE_0");
    }
  else if(st==WeaponState::W1H || st==WeaponState::W2H) {
    if(a==Anim::Atack && (pose.isInAnim("S_1HRUNL") || pose.isInAnim("S_2HRUNL")))
      return solveFrm("T_%sATTACKMOVE",st);
    if(a==Anim::AtackL)
      return solveFrm("T_%sATTACKL",st);
    if(a==Anim::AtackR)
      return solveFrm("T_%sATTACKR",st);
    if(a==Anim::Atack || a==Anim::AtackL || a==Anim::AtackR)
      return solveFrm("S_%sATTACK",st); // TODO: proper atack  window
    if(a==Anim::AtackBlock) {
      const Animation::Sequence* s=nullptr;
      switch(std::rand()%3){
        case 0: s = solveFrm("T_%sPARADE_0",   st); break;
        case 1: s = solveFrm("T_%sPARADE_0_A2",st); break;
        case 2: s = solveFrm("T_%sPARADE_0_A3",st); break;
        }
      if(s==nullptr)
        s = solveFrm("T_%sPARADE_0",st);
      return s;
      }
    if(a==Anim::AtackFinish)
      return solveFrm("T_%sSFINISH",st);
    }
  else if(st==WeaponState::Bow || st==WeaponState::CBow) {
    if(a==Anim::AimBow) {
      if(!pose.isInAnim("T_BOWRELOAD") && !pose.isInAnim("T_CBOWRELOAD") &&
         !pose.isInAnim("S_BOWSHOOT")  && !pose.isInAnim("S_CBOWSHOOT"))
        return solveFrm("T_%sRUN_2_%sAIM",st);
      return solveFrm("S_%sSHOOT",st);
      }
    if(a==Anim::Atack)
      return solveFrm("T_%sRELOAD",st);
    if(a!=Anim::AimBow &&
       (pose.isInAnim("T_BOWRELOAD")       || pose.isInAnim("T_CBOWRELOAD") ||
        pose.isInAnim("T_BOWRUN_2_BOWAIM") || pose.isInAnim("T_CBOWRUN_2_CBOWAIM")))
      return solveFrm("T_%sAIM_2_%sRUN",st);
    }
  else if(st==WeaponState::Mage) {
    // Magic-cast
    if(Anim::MagFirst<=a && a<=Anim::MagLast) {
      if(pose.isInAnim("S_MAGRUN"))
        return solveMag("T_MAGRUN_2_%sSHOOT",a); else
        return solveMag("S_%sSHOOT",a);
      }
    for(uint8_t i=Anim::MagFirst;i<=Anim::MagLast;++i) {
      auto sq = solveMag("S_%sSHOOT",Anim(i));
      if(pose.isInAnim(sq))
        return solveMag("T_%sSHOOT_2_STAND",Anim(i));
      }
    }
  if(a==Anim::MagNoMana)
    return solveFrm("T_CASTFAIL");
  // Item-use
  if(Anim::Food1<=a && a<=Anim::Rice2) {
    for(auto& i:schemes){
      if(a!=i.second)
        continue;
      char S_ID_S0[128]={};
      std::snprintf(S_ID_S0,sizeof(S_ID_S0),"S_%s_S0",i.first);

      if(a==i.second && !pose.isInAnim(S_ID_S0))
        return solveItemUse("T_%s_STAND_2_S0",i.first);
      if(a<Anim::IdleLast && pose.isInAnim(S_ID_S0))
        return solveItemUse("T_%s_S0_2_STAND",i.first);
      if(a==i.second)
        return solveItemUse("S_%s_S0",i.first);
      }
    }
  // Move
  if(a==Idle) {
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("S_SWIM");
    if(bool(wlkMode&WalkBit::WM_Walk))
      return solveFrm("S_%sWALK",st);
    return solveFrm("S_%sRUN",st);
    }
  if(a==Move) {
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("S_SWIMF",st);
    if(bool(wlkMode & WalkBit::WM_Walk))
      return solveFrm("S_%sWALKL",st);
    if(bool(wlkMode & WalkBit::WM_Water))
      return solveFrm("S_%sWALKWL",st);
    return solveFrm("S_%sRUNL",st);
    }
  if(a==MoveL) {
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("S_SWIM"); // ???
    if(bool(wlkMode & WalkBit::WM_Walk))
      return solveFrm("T_%sWALKWSTRAFEL",st);
    if(bool(wlkMode & WalkBit::WM_Water))
      return solveFrm("T_%sWALKWSTRAFEL",st);
    return solveFrm("T_%sRUNSTRAFEL",st);
    }
  if(a==MoveR) {
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("S_SWIM"); // ???
    if(bool(wlkMode & WalkBit::WM_Walk))
      return solveFrm("T_%sWALKWSTRAFER",st);
    if(bool(wlkMode & WalkBit::WM_Water))
      return solveFrm("T_%sWALKWSTRAFER",st);
    return solveFrm("T_%sRUNSTRAFER",st);
    }
  if(a==Anim::MoveBack) {
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("S_SWIMB");
    return solveFrm("T_%sJUMPB",st);
    }
  // Rotation
  if(a==RotL) {
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("T_SWIMTURNL");
    if(bool(wlkMode & WalkBit::WM_Walk))
      return solveFrm("T_%sWALKTURNL",st);
    if(bool(wlkMode & WalkBit::WM_Water))
      return solveFrm("T_%sWALKWTURNL",st);
    return solveFrm("T_%sRUNTURNL",st);
    }
  if(a==RotR) {
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("T_SWIMTURNR");
    if(bool(wlkMode & WalkBit::WM_Walk))
      return solveFrm("T_%sWALKTURNR",st);
    if(bool(wlkMode & WalkBit::WM_Water))
      return solveFrm("T_%sWALKWTURNR",st);
    return solveFrm("T_%sRUNTURNR",st);
    }
  // Jump regular
  /*
  if(cur==Anim::Move && a==Jump)
    return animSequence("T_RUNL_2_JUMP");
  if(cur==Anim::Idle && a==Anim::Jump)
    return animSequence("T_STAND_2_JUMP");
  if(cur==Anim::Jump && a==Anim::Idle)
    return animSequence("T_JUMP_2_STAND");
  if(a==Anim::Jump)
    return animSequence("S_JUMP");
  if(cur==Anim::Fall && a==Move)
    return animSequence("T_RUN_2_RUNL");
    */
  if(a==Jump) {
    if(pose.isIdle())
      return solveFrm("T_STAND_2_JUMP");
    return solveFrm("S_JUMP");
    /*
    if(!pose.isInAnim("S_JUMP")) {
      if(pose.isIdle())
        return solveFrm("T_STAND_2_JUMP"); else
        return solveFrm("T_RUNL_2_JUMP");
      }
    return solveFrm("S_JUMP");*/
    }
  if(a==Anim::Fall)
    return solveFrm("S_FALLDN");
  if(a==Anim::DeadA) {
    if(pose.isInAnim("S_WOUNDED") || pose.isInAnim("S_WOUNDEDB"))
      return solveDead("T_WOUNDED_2_DEAD","T_WOUNDEDB_2_DEADB"); else
      return solveDead("T_DEAD", "T_DEADB");
    }
  if(a==Anim::DeadB) {
    if(pose.isInAnim("S_WOUNDED") || pose.isInAnim("S_WOUNDEDB"))
      return solveDead("T_WOUNDEDB_2_DEADB","T_WOUNDED_2_DEAD"); else
      return solveDead("T_DEADB","T_DEAD");
    }
  if(a==Anim::UnconsciousA)
    return solveFrm("S_WOUNDED");
  if(a==Anim::UnconsciousB)
    return solveFrm("S_WOUNDEDB");

  if(auto sq = solveAmbient(a))
    return sq;

  return nullptr;
  }

const Animation::Sequence *AnimationSolver::solveAnim(WeaponState st, WeaponState cur, const Pose &pose) const {
  // Weapon draw/undraw
  if(st==cur)
    return nullptr;
  if(st==WeaponState::NoWeapon){
    if(pose.isInAnim("S_1HRUNL")   || pose.isInAnim("S_2HRUNL") ||
       pose.isInAnim("S_BOWRUNL")  || pose.isInAnim("S_CBOWRUNL") ||
       pose.isInAnim("S_FISTRUNL") || pose.isInAnim("S_MAGRUNL"))
      return solveFrm("T_%sMOVE_2_MOVE",cur);
    return solveFrm("T_%sRUN_2_%s",cur);
    }
  if(st==WeaponState::Fist ||
     st==WeaponState::W1H  || st==WeaponState::W2H ||
     st==WeaponState::Bow  || st==WeaponState::CBow ||
     st==WeaponState::Mage){
    if(pose.isInAnim("S_RUNL"))
      return solveFrm("T_MOVE_2_%sMOVE",st);
    return solveFrm("T_%s_2_%sRUN",st);
    }
  return nullptr;
  }

const Animation::Sequence *AnimationSolver::solveAnim(Interactive *inter, AnimationSolver::Anim a, const Pose &) const {
  if(inter==nullptr)
    return nullptr;
  if(a==AnimationSolver::InteractOut) {
    return inter->anim(*this,Interactive::Out);
    } else {
    return inter->anim(*this,Interactive::In);
    }
  }

const Animation::Sequence *AnimationSolver::solveFrm(const char *format, WeaponState st) const {
  static const char* weapon[] = {
    "",
    "FIST",
    "1H",
    "2H",
    "BOW",
    "CBOW",
    "MAG"
    };
  char name[128]={};
  std::snprintf(name,sizeof(name),format,weapon[int(st)],weapon[int(st)]);
  if(auto ret=solveFrm(name))
    return ret;
  std::snprintf(name,sizeof(name),format,"");
  if(auto ret=solveFrm(name))
    return ret;
  std::snprintf(name,sizeof(name),format,"FIST");
  return solveFrm(name);
  }

const Animation::Sequence *AnimationSolver::solveFrm(const char *name) const {
  if(name==nullptr || name[0]=='\0')
    return nullptr;
  for(size_t i=overlay.size();i>0;){
    --i;
    if(auto s = overlay[i].skeleton->sequence(name))
      return s;
    }
  return baseSk ? baseSk->sequence(name) : nullptr;
  }

const Animation::Sequence* AnimationSolver::solveMag(const char *format,Anim spell) const {
  static const char* mg[]={"FIB", "WND", "HEA", "PYR", "FEA", "TRF", "SUM", "MSD", "STM", "FRZ", "SLE", "WHI", "SCK", "FBT"};

  char name[128]={};
  std::snprintf(name,sizeof(name),format,mg[spell-Anim::MagFirst]);
  if(auto a=solveFrm(name))
    return a;
  std::snprintf(name,sizeof(name),format,mg[0]);
  return solveFrm(name);
  }

const Animation::Sequence *AnimationSolver::solveDead(const char *format1, const char *format2) const {
  if(auto a=solveFrm(format1))
    return a;
  return solveFrm(format2);
  }

const Animation::Sequence* AnimationSolver::solveItemUse(const char *format, const char *scheme) const {
  char buf[128]={};
  std::snprintf(buf,sizeof(buf),format,scheme);
  return solveFrm(buf);
  }

const Animation::Sequence *AnimationSolver::solveAmbient(AnimationSolver::Anim a) const {
  if(a==Anim::GuardL)
    return solveFrm("S_LGUARD");
  if(a==Anim::GuardH)
    return solveFrm("S_HGUARD");
  if(a==Anim::Sit)
    return solveFrm("S_SIT");
  if(a==Anim::Sleep)
    return solveFrm("S_SLEEP");
  if(a==Anim::GuardSleep)
    return solveFrm("S_GUARDSLEEP");
  if(a==Anim::MagicSleep)
    return solveFrm("S_VICTIM_SLE");
  if(a==Anim::Pray)
    return solveFrm("S_PRAY");
  if(a==Anim::PrayRand)
    return solveFrm("T_PRAY_RANDOM");
  if(a==Anim::Talk)
    return solveFrm("S_TALK");
  if(a==Anim::GuardLChLeg)
    return solveFrm("T_LGUARD_CHANGELEG");
  if(a==Anim::GuardLScratch)
    return solveFrm("T_LGUARD_SCRATCH");
  if(a==Anim::GuardLStrectch)
    return solveFrm("T_LGUARD_STRETCH");
  if(a==Anim::Lookaround)
    return solveFrm("T_HGUARD_LOOKAROUND");
  if(a==Anim::Perception)
    return solveFrm("T_PERCEPTION");
  if(a==Anim::Chair1)
    return solveFrm("R_CHAIR_RANDOM_1");
  if(a==Anim::Chair2)
    return solveFrm("R_CHAIR_RANDOM_2");
  if(a==Anim::Chair3)
    return solveFrm("R_CHAIR_RANDOM_3");
  if(a==Anim::Chair4)
    return solveFrm("R_CHAIR_RANDOM_4");
  if(a==Anim::Roam1)
    return solveFrm("R_ROAM1");
  if(a==Anim::Roam2)
    return solveFrm("R_ROAM2");
  if(a==Anim::Roam3)
    return solveFrm("R_ROAM3");
  if(a==Anim::Food1)
    return solveFrm("T_FOOD_RANDOM_1");
  if(a==Anim::Food2)
    return solveFrm("T_FOOD_RANDOM_2");
  if(a==Anim::FoodHuge1)
    return solveFrm("T_FOODHUGE_RANDOM_1");
  if(a==Anim::Potition1)
    return solveFrm("T_POTION_RANDOM_1");
  if(a==Anim::Potition2)
    return solveFrm("T_POTION_RANDOM_2");
  if(a==Anim::Potition3)
    return solveFrm("T_POTION_RANDOM_3");
  if(Anim::Dance1<=a && a<=Anim::Dance9){
    char buf[32]={};
    std::snprintf(buf,sizeof(buf),"T_DANCE_%02d",a-Anim::Dance1+1);
    return solveFrm(buf);
    }
  if(Anim::Dialog1<=a && a<=Anim::Dialog21){
    char buf[32]={};
    std::snprintf(buf,sizeof(buf),"T_DIALOGGESTURE_%02d",int(a-Anim::Dialog1+1));
    return solveFrm(buf);
    }
  if(Anim::Fear1<=a && a<=Anim::Fear3){
    char buf[32]={};
    std::snprintf(buf,sizeof(buf),"S_FEAR_VICTIM%d",int(a-Anim::Fear1+1));
    return solveFrm(buf);
    }
  if(a==Anim::Plunder)
    return solveFrm("T_PLUNDER");
  if(a==Anim::Pee)
    return solveFrm("S_PEE");
  if(a==Anim::Eat)
    return solveFrm("S_EAT");
  if(a==Anim::Warn)
    return solveFrm("T_WARN");
  if(a==Anim::Training)
    return solveFrm("T_1HSFREE");
  return nullptr;
  }

AnimationSolver::Anim AnimationSolver::animByName(const std::string &name) const {
  if(name=="T_HEASHOOT_2_STAND" || name=="T_LGUARD_2_STAND" || name=="T_HGUARD_2_STAND" ||
     name=="T_EAT_2_STAND"      || name=="T_SLEEP_2_STAND"  || name=="T_GUARDSLEEP_2_STAND" ||
     name=="T_SIT_2_STAND"      || name=="T_PEE_2_STAND"    || name=="T_VICTIM_SLE_2_STAND" ||
     name=="T_TALK_2_STAND")
    return Anim::Idle;

  if(name=="T_STAND_2_LGUARD" || name=="S_LGUARD")
    return Anim::GuardL;
  if(name=="T_STAND_2_HGUARD")
    return Anim::GuardH;
  if(name=="T_STAND_2_TALK" || name=="S_TALK")
    return Anim::Talk;
  if(name=="T_PERCEPTION")
    return Anim::Perception;
  if(name=="T_HGUARD_LOOKAROUND")
    return Anim::Lookaround;
  if(name=="T_STAND_2_EAT" || name=="S_EAT")
    return Anim::Eat;
  if(name=="T_STAND_2_SLEEP" || name=="S_SLEEP")
    return Anim::Sleep;
  if(name=="T_STAND_2_GUARDSLEEP" || name=="S_GUARDSLEEP")
    return Anim::GuardSleep;
  if(name=="T_STAND_2_SIT" || name=="S_SIT")
    return Anim::Sit;
  if(name=="T_LGUARD_CHANGELEG")
    return Anim::GuardLChLeg;
  if(name=="T_LGUARD_STRETCH")
    return Anim::GuardLStrectch;
  if(name=="T_LGUARD_SCRATCH")
    return Anim::GuardLScratch;
  if(name=="T_1HSFREE")
    return Anim::Training;
  if(name=="T_MAGRUN_2_HEASHOOT" || name=="S_HEASHOOT")
    return Anim::MagHea;
  if(name=="T_WARN")
    return Anim::Warn;
  if(name=="R_CHAIR_RANDOM_1")
    return Anim::Chair1;
  if(name=="R_CHAIR_RANDOM_2")
    return Anim::Chair2;
  if(name=="R_CHAIR_RANDOM_3")
    return Anim::Chair3;
  if(name=="R_CHAIR_RANDOM_4")
    return Anim::Chair4;
  if(name=="R_ROAM1")
    return Anim::Roam1;
  if(name=="R_ROAM2")
    return Anim::Roam2;
  if(name=="R_ROAM3")
    return Anim::Roam3;
  if(name=="T_FOOD_RANDOM_1")
    return Anim::Food1;
  if(name=="T_FOOD_RANDOM_2")
    return Anim::Food2;
  if(name=="T_FOODHUGE_RANDOM_1")
    return Anim::FoodHuge1;
  if(name=="T_POTION_RANDOM_1")
    return Anim::Potition1;
  if(name=="T_POTION_RANDOM_2")
    return Anim::Potition2;
  if(name=="T_POTION_RANDOM_3")
    return Anim::Potition3;
  if(name=="T_JOINT_RANDOM_1")
    return Anim::Joint1;
  if(name=="T_MEAT_RANDOM_1")
    return Anim::Meat1;
  if(name=="T_STAND_2_PEE" || name=="S_PEE")
    return Anim::Pee;
  if(name=="T_DANCE_01")
    return Anim::Dance1;
  if(name=="T_DANCE_02")
    return Anim::Dance2;
  if(name=="T_DANCE_03")
    return Anim::Dance3;
  if(name=="T_DANCE_04")
    return Anim::Dance4;
  if(name=="T_DANCE_05")
    return Anim::Dance5;
  if(name=="T_DANCE_06")
    return Anim::Dance6;
  if(name=="T_DANCE_07")
    return Anim::Dance7;
  if(name=="T_DANCE_08")
    return Anim::Dance8;
  if(name=="T_DANCE_09")
    return Anim::Dance9;
  if(name=="T_STAND_2_FEAR_VICTIM1" || name=="S_FEAR_VICTIM1")
    return Anim::Fear1;
  if(name=="T_STAND_2_FEAR_VICTIM2" || name=="S_FEAR_VICTIM2")
    return Anim::Fear2;
  if(name=="T_STAND_2_FEAR_VICTIM3" || name=="S_FEAR_VICTIM3")
    return Anim::Fear3;
  if(name=="T_STAND_2_VICTIM_SLE" || name=="S_VICTIM_SLE")
    return Anim::MagicSleep;
  if(name=="T_STAND_2_PRAY")
    return Anim::Pray;
  if(name=="T_PRAY_RANDOM")
    return Anim::PrayRand;
  if(name=="T_PLUNDER")
    return Anim::Plunder;
  if(name=="S_FOOD_S0")
    return Anim::Food1;
  if(name=="S_FOODHUGE_S0")
    return Anim::FoodHuge1;
  if(name=="S_POTIONFAST_S0")
    return Anim::PotitionFast;
  if(name=="S_POTION_S0")
    return Anim::Potition1;
  if(name.find("T_DIALOGGESTURE_")==0){
    for(int i=Anim::Dialog1;i<=DialogLastG1;++i){
      char buf[32]={};
      std::snprintf(buf,sizeof(buf),"T_DIALOGGESTURE_%02d",int(i-Anim::Dialog1+1));
      if(name==buf)
        return Anim(i);
      }
    }
  return Anim::NoAnim;
  }
