#include "animationsolver.h"

#include "world/objects/interactive.h"
#include "world/world.h"
#include "game/serialize.h"
#include "utils/fileext.h"
#include "gothic.h"
#include "skeleton.h"
#include "pose.h"
#include "resources.h"

using namespace Tempest;

AnimationSolver::AnimationSolver() {
  }

void AnimationSolver::save(Serialize &fout) const {
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
  invalidateCache();
  }

void AnimationSolver::setSkeleton(const Skeleton *sk) {
  baseSk = sk;
  invalidateCache();
  }

bool AnimationSolver::hasOverlay(const Skeleton* sk) const {
  for(auto& i:overlay)
    if(i.skeleton==sk)
      return true;
  return false;
  }

void AnimationSolver::addOverlay(const Skeleton* sk, uint64_t time) {
  if(sk==nullptr)
    return;
  // incompatible overlay
  if(baseSk==nullptr || sk->nodes.size()!=baseSk->nodes.size())
    return;
  Overlay ov;
  ov.skeleton = sk;
  ov.time     = time;
  overlay.push_back(ov);
  invalidateCache();
  }

void AnimationSolver::delOverlay(std::string_view sk) {
  if(overlay.size()==0)
    return;
  auto skelet = Resources::loadSkeleton(sk);
  delOverlay(skelet);
  }

void AnimationSolver::delOverlay(const Skeleton *sk) {
  for(size_t i=0;i<overlay.size();++i)
    if(overlay[i].skeleton==sk){
      overlay.erase(overlay.begin()+int(i));
      invalidateCache();
      return;
      }
  }

void AnimationSolver::clearOverlays() {
  overlay.clear();
  invalidateCache();
  }

void AnimationSolver::update(uint64_t tickCount) {
  for(size_t i=0;i<overlay.size();){
    auto& ov = overlay[i];
    if(ov.time!=0 && ov.time<tickCount) {
      overlay.erase(overlay.begin()+int(i));
      invalidateCache();
      } else {
      ++i;
      }
    }
  }

const Animation::Sequence* AnimationSolver::solveAnim(AnimationSolver::Anim a, WeaponState st, WalkBit wlkMode, const Pose& pose) const {
  if(0<a && a<=AnimationSolver::CacheLast && int(st)<2 && int(wlkMode)<2) {
    auto& ptr = cache[a-1][int(st)][int(wlkMode)];
    if(ptr==nullptr)
      ptr = implSolveAnim(a,st,wlkMode,pose);
    return ptr;
    }
  return implSolveAnim(a,st,wlkMode,pose);
  }

const Animation::Sequence* AnimationSolver::implSolveAnim(AnimationSolver::Anim a, WeaponState st, WalkBit wlkMode, const Pose& pose) const {
  // Attack
  if(st==WeaponState::Fist) {
    if(a==Anim::Attack) {
      if(pose.isInAnim("S_FISTRUNL"))
        return solveFrm("T_FISTATTACKMOVE");
      return solveFrm("S_FISTATTACK");
      }
    if(a==Anim::AttackBlock) {
      bool g2 = Gothic::inst().version().game==2;
      return g2 ? solveFrm("T_FISTPARADE_0") : solveFrm("T_FISTPARADE_O");
      }
    }
  else if(st==WeaponState::W1H || st==WeaponState::W2H) {
    if(a==Anim::Attack && pose.hasState(BS_RUN))
      return solveFrm("T_%sATTACKMOVE",st);
    if(a==Anim::AttackL)
      return solveFrm("T_%sATTACKL",st);
    if(a==Anim::AttackR)
      return solveFrm("T_%sATTACKR",st);
    if(a==Anim::Attack || a==Anim::AttackL || a==Anim::AttackR)
      return solveFrm("S_%sATTACK",st);
    if(a==Anim::AttackBlock) {
      bool g2 = Gothic::inst().version().game==2;
      if(g2) {
        const Animation::Sequence* s=nullptr;
        switch(std::rand()%3) {
          case 0: s = solveFrm("T_%sPARADE_0",   st); break;
          case 1: s = solveFrm("T_%sPARADE_0_A2",st); break;
          case 2: s = solveFrm("T_%sPARADE_0_A3",st); break;
          }
        if(s==nullptr)
          s = solveFrm("T_%sPARADE_0",st);
        return s;
        } else {
        return solveFrm("T_%sPARADE_O",st);
        }
      }
    if(a==Anim::AttackFinish)
      return solveFrm("T_%sSFINISH",st);
    }
  else if(st==WeaponState::Bow || st==WeaponState::CBow) {
    // S_BOWAIM -> S_BOWSHOOT+T_BOWRELOAD -> S_BOWAIM
    if(a==AimBow) {
      auto bs = pose.bodyState();
      if(bs==BS_HIT)
        return solveFrm("T_%sRELOAD",st);
      if(bs==BS_AIMNEAR || bs==BS_AIMFAR || pose.isStanding())
        return solveFrm("S_%sAIM",st);
      return solveFrm("S_%sRUN",st);
      }
    if(a==Attack) {
      auto bs = pose.bodyState();
      if(bs==BS_AIMNEAR || bs==BS_AIMFAR)
        return solveFrm("S_%sSHOOT",st);
      }
    }

  if(a==MagNoMana)
    return solveFrm("T_CASTFAIL");
  // Move
  if(a==Idle) {
    const Animation::Sequence* s = nullptr;
    if(bool(wlkMode & WalkBit::WM_Dive))
      s = solveFrm("S_DIVE");
    else if(bool(wlkMode & WalkBit::WM_Swim))
      s = solveFrm("S_SWIM");
    else if(bool(wlkMode&WalkBit::WM_Sneak))
      s = solveFrm("S_%sSNEAK",st);
    else if(bool(wlkMode&WalkBit::WM_Walk))
      s = solveFrm("S_%sWALK",st);
    else
      s = solveFrm("S_%sRUN",st);

    if(s==nullptr) {
      // make sure that 'Idle' has something at least
      s = solveFrm("S_%sWALK",st);
      }
    return s;
    }
  if(a==Move)  {
    if(bool(wlkMode & WalkBit::WM_Dive)) {
      if(pose.bodyState()==BS_DIVE)
        return solveFrm("S_DIVEF",st); else
        return solveFrm("S_DIVE");
      }
    const Animation::Sequence* s = nullptr;
    if(bool(wlkMode & WalkBit::WM_Swim))
      s = solveFrm("S_SWIMF",st);
    else if(bool(wlkMode & WalkBit::WM_Sneak))
      s = solveFrm("S_%sSNEAKL",st);
    else if(bool(wlkMode & WalkBit::WM_Walk))
      s = solveFrm("S_%sWALKL",st);
    else if(bool(wlkMode & WalkBit::WM_Water))
      s = solveFrm("S_%sWALKWL",st);
    if(s!=nullptr)
      return s;
    return solveFrm("S_%sRUNL",st);
    }
  if(a==MoveL) {
    if(bool(wlkMode & WalkBit::WM_Dive))
      return solveFrm("S_DIVE"); // ???
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("S_SWIM"); // ???
    if(bool(wlkMode & WalkBit::WM_Sneak))
      return solveFrm("T_%sSNEAKSTRAFEL",st);
    if(bool(wlkMode & WalkBit::WM_Walk))
      return solveFrm("T_%sWALKWSTRAFEL",st);
    if(bool(wlkMode & WalkBit::WM_Water))
      return solveFrm("T_%sWALKWSTRAFEL",st);
    return solveFrm("T_%sRUNSTRAFEL",st);
    }
  if(a==MoveR) {
    if(bool(wlkMode & WalkBit::WM_Dive))
      return solveFrm("S_DIVE"); // ???
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("S_SWIM"); // ???
    if(bool(wlkMode & WalkBit::WM_Sneak))
      return solveFrm("T_%sSNEAKSTRAFER",st);
    if(bool(wlkMode & WalkBit::WM_Walk))
      return solveFrm("T_%sWALKWSTRAFER",st);
    if(bool(wlkMode & WalkBit::WM_Water))
      return solveFrm("T_%sWALKWSTRAFER",st);
    return solveFrm("T_%sRUNSTRAFER",st);
    }
  if(a==MoveBack) {
    const Animation::Sequence* s = nullptr;
    if(bool(wlkMode & WalkBit::WM_Dive))
      s = solveFrm("S_DIVE");
    else if(bool(wlkMode & WalkBit::WM_Swim))
      s = solveFrm("S_SWIMB");
    else if(bool(wlkMode & WalkBit::WM_Sneak))
      s = solveFrm("S_%sSNEAKBL",st);
    else if(st==WeaponState::Fist)
      s = solveFrm("T_%sPARADEJUMPB",st);
    if(s!=nullptr)
      return s;
    // This is bases on original game: if no move-back animation, even in water, game defaults to standard walk-back
    return solveFrm("T_%sJUMPB",st);
    }
  // Rotation
  if(a==RotL) {
    if(bool(wlkMode & WalkBit::WM_Dive))
      return solveFrm("T_DIVETURNL");
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("T_SWIMTURNL");
    if(bool(wlkMode & WalkBit::WM_Sneak))
      return solveFrm("T_SNEAKTURNL");
    if(bool(wlkMode & WalkBit::WM_Walk))
      return solveFrm("T_%sWALKTURNL",st);
    if(bool(wlkMode & WalkBit::WM_Water))
      return solveFrm("T_%sWALKWTURNL",st);
    return solveFrm("T_%sRUNTURNL",st);
    }
  if(a==RotR) {
    if(bool(wlkMode & WalkBit::WM_Dive))
      return solveFrm("T_DIVETURNR");
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("T_SWIMTURNR");
    if(bool(wlkMode & WalkBit::WM_Sneak))
      return solveFrm("T_SNEAKTURNR");
    if(bool(wlkMode & WalkBit::WM_Walk))
      return solveFrm("T_%sWALKTURNR",st);
    if(bool(wlkMode & WalkBit::WM_Water))
      return solveFrm("T_%sWALKWTURNR",st);
    return solveFrm("T_%sRUNTURNR",st);
    }
  // Whirl around
  if(a==WhirlL) {
    // Whirling in water should not happen, but just to make sure
    if(bool(wlkMode & WalkBit::WM_Dive))
      return solveFrm("T_DIVETURNL");
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("T_SWIMTURNL");

    // whirling is only used in G1, but the script function is present
    // in all games. In G2 all models except the human also have the
    // needed animation.
    const Animation::Sequence* s = solveFrm("T_SURPRISE_CCW");
    if(s==nullptr)
      s = solveFrm("T_%sRUNTURNL",st);
    return s;
    }
  if(a==WhirlR) {
    // Whirling in water should not happen, but just to make sure
    if(bool(wlkMode & WalkBit::WM_Dive))
      return solveFrm("T_DIVETURNR");
    if(bool(wlkMode & WalkBit::WM_Swim))
      return solveFrm("T_SWIMTURNR");

    const Animation::Sequence* s = solveFrm("T_SURPRISE_CW");
    if(s==nullptr)
      s = solveFrm("T_%sRUNTURNR",st);
    return s;
    }
  // Jump regular
  if(a==Jump)
    return solveFrm("S_JUMP");
  if(a==JumpUpLow)
    return solveFrm("S_JUMPUPLOW");
  if(a==JumpUpMid)
    return solveFrm("S_JUMPUPMID");
  if(a==JumpUp)
    return solveFrm("S_JUMPUP");

  if(a==JumpHang) {
    if(pose.bodyState()==BS_JUMP) {
      if(auto ret = solveFrm("T_JUMPUP_2_HANG"))
        return ret;
      }
    //return solveFrm("S_HANG");
    return solveFrm("T_HANG_2_STAND");
    }

  if(a==Anim::Fall)
    return solveFrm("S_FALLDN");

  if(a==Anim::Fallen) {
    if(pose.isInAnim("S_FALL") || pose.isInAnim("S_FALLEN"))
      return solveFrm("S_FALLEN");
    if(pose.isInAnim("S_FALLB") || pose.isInAnim("S_FALLENB"))
      return solveFrm("S_FALLENB");
    return solveFrm("S_FALLEN");
    }
  if(a==Anim::FallenA)
    return solveFrm("S_FALLEN");
  if(a==Anim::FallenB)
    return solveFrm("S_FALLENB");

  if(a==Anim::FallDeep) {
    if(pose.bodyState()==BS_FALL || pose.bodyState()==BS_JUMP)
      return solveFrm("S_FALL");
    return solveFrm("S_FALLB");
    }
  if(a==Anim::FallDeepA)
    return solveFrm("S_FALL");
  if(a==Anim::FallDeepB)
    return solveFrm("S_FALLB");

  if(a==Anim::SlideA)
    return solveFrm("S_SLIDE");
  if(a==Anim::SlideB)
    return solveFrm("S_SLIDEB");
  if(a==Anim::StumbleA)
    return solveFrm("T_STUMBLE");
  if(a==Anim::StumbleB)
    return solveFrm("T_STUMBLEB");
  if(a==Anim::DeadA) {
    if(pose.isInAnim("S_WOUNDED")  || pose.isInAnim("T_STAND_2_WOUNDED") ||
       pose.isInAnim("S_WOUNDEDB") || pose.isInAnim("T_STAND_2_WOUNDEDB"))
      return solveDead("T_WOUNDED_2_DEAD","T_WOUNDEDB_2_DEADB");
    if(pose.bodyState()==BS_FALL)
      return solveDead("T_DEAD", "T_DEADB");
    if(pose.hasAnim())
      return solveDead("T_DEAD", "T_DEADB");
    return solveDead("S_DEAD", "S_DEADB");
    }
  if(a==Anim::DeadB) {
    if(pose.isInAnim("S_WOUNDED")  || pose.isInAnim("T_STAND_2_WOUNDED") ||
       pose.isInAnim("S_WOUNDEDB") || pose.isInAnim("T_STAND_2_WOUNDEDB"))
      return solveDead("T_WOUNDEDB_2_DEADB","T_WOUNDED_2_DEAD");
    if(pose.hasAnim())
      return solveDead("T_DEADB","T_DEAD"); else
      return solveDead("S_DEADB","S_DEAD");
    }

  if(a==Anim::UnconsciousA)
    return solveFrm("T_STAND_2_WOUNDED");
  if(a==Anim::UnconsciousB)
    return solveFrm("T_STAND_2_WOUNDEDB");

  if(a==Anim::ItmGet)
    return solveFrm("S_IGET");
  if(a==Anim::ItmDrop)
    return solveFrm("S_IDROP");
  if(a==Anim::PointAt)
    return solveFrm("T_POINT");

  return nullptr;
  }

const Animation::Sequence *AnimationSolver::solveAnim(WeaponState st, WeaponState cur, bool run) const {
  // Weapon draw/undraw
  if(st==cur)
    return nullptr;
  switch(st) {
    case WeaponState::NoWeapon:
      if(run)
        return solveFrm("T_%sMOVE_2_MOVE",cur);
      return solveFrm("T_%sRUN_2_%s",cur);
    case WeaponState::Fist:
    case WeaponState::Mage:
    case WeaponState::W1H:
    case WeaponState::W2H:
    case WeaponState::Bow:
    case WeaponState::CBow:
      if(run)
        return solveFrm("T_MOVE_2_%sMOVE",st);
      return solveFrm("T_%s_2_%sRUN",st);
    }
  return nullptr;
  }

const Animation::Sequence* AnimationSolver::solveAnim(std::string_view scheme, bool run, bool invest) const {
  // example: "T_MAGWALK_2_FBTSHOOT"

  string_frm name("");
  if(run && invest)
    name = string_frm("T_MAGMOVE_2_",scheme,"CAST");
  else if(run)
    name = string_frm("T_MAGMOVE_2_",scheme,"SHOOT");
  else if(invest)
    name = string_frm("T_MAGRUN_2_",scheme,"CAST");
  else
    name = string_frm("T_MAGRUN_2_",scheme,"SHOOT");

  if(auto sq = solveFrm(name))
    return sq;

  name = string_frm("S_",scheme,"SHOOT");
  return solveFrm(name);
  }

const Animation::Sequence *AnimationSolver::solveAnim(Interactive *inter, AnimationSolver::Anim a, const Pose &) const {
  if(inter==nullptr)
    return nullptr;
  switch(a) {
    case AnimationSolver::InteractIn:
      return inter->animNpc(*this,Interactive::In);
    case AnimationSolver::InteractOut:
      return inter->animNpc(*this,Interactive::Out);
    case AnimationSolver::InteractToStand:
      return inter->animNpc(*this,Interactive::ToStand);
    case AnimationSolver::InteractFromStand:
      return inter->animNpc(*this,Interactive::FromStand);
    default:
      return nullptr;
    }
  }

const Animation::Sequence* AnimationSolver::solveFrm(std::string_view fview, WeaponState st) const {
  char format[256] = {};
  std::snprintf(format,sizeof(format),"%.*s",int(fview.size()),fview.data());

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
  std::snprintf(name,sizeof(name),format,"","");
  if(auto ret=solveFrm(name))
    return ret;
  std::snprintf(name,sizeof(name),format,"FIST","");
  return solveFrm(name);
  }

const Animation::Sequence* AnimationSolver::solveMag(std::string_view fview, std::string_view spell) const {
  char format[256] = {};
  std::snprintf(format,sizeof(format),"%.*s",int(fview.size()),fview.data());

  char name[128]={};
  std::snprintf(name,sizeof(name),format,std::string(spell).c_str()); //FIXME
  return solveFrm(name);
  }

const Animation::Sequence *AnimationSolver::solveDead(std::string_view format1, std::string_view format2) const {
  if(auto a=solveFrm(format1))
    return a;
  return solveFrm(format2);
  }

void AnimationSolver::invalidateCache() {
  std::memset(cache,0,sizeof(cache));
  }

const Animation::Sequence* AnimationSolver::solveNext(const Animation::Sequence& sq) const {
  if(sq.next.empty())
    return nullptr;
  std::string_view name = sq.next;
  for(size_t i=overlay.size();i>0;){
    --i;
    if(overlay[i].skeleton->animation()==sq.owner && sq.nextPtr!=nullptr)
      return sq.nextPtr; // fast-forward path
    if(auto s = overlay[i].skeleton->sequence(name))
      return s;
    }
  if(baseSk->animation()==sq.owner)
    return sq.nextPtr; // fast-forward path
  return baseSk ? baseSk->sequence(name) : nullptr;
  }

const Animation::Sequence *AnimationSolver::solveFrm(std::string_view name) const {
  if(name.empty())
    return nullptr;

  for(size_t i=overlay.size();i>0;){
    --i;
    if(auto s = overlay[i].skeleton->sequence(name))
      return s;
    }
  if(baseSk==nullptr)
    return nullptr;
  return baseSk->sequence(name);
  }
