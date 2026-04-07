#include "movealgo.h"

#include <Tempest/Log>

#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/world.h"
#include "serialize.h"

const float   MoveAlgo::closeToPointThreshold = 40;
const float   MoveAlgo::climbMove             = 55;
const float   MoveAlgo::gravity               = DynamicWorld::gravity;
const float   MoveAlgo::eps                   = 2.f;   // 2-santimeters
const int32_t MoveAlgo::flyOverWaterHint      = 999999;
const float   MoveAlgo::waterPadd             = 15;

MoveAlgo::MoveAlgo(Npc& unit)
  :npc(unit) {
  }

void MoveAlgo::load(Serialize &fin) {
  fin.read(reinterpret_cast<uint32_t&>(flags));
  fin.read(mulSpeed,fallSpeed,fallCount,climbStart,climbPos0,climbHeight);
  fin.read(reinterpret_cast<uint8_t&>(jmp));

  std::string str;
  fin.read(str);
  portal       = npc.world().physic()->validateSectorName(str);
  fin.read(str);
  formerPortal = npc.world().physic()->validateSectorName(str);

  fin.read(diveStart);
  }

void MoveAlgo::save(Serialize &fout) const {
  fout.write(uint32_t(flags));
  fout.write(mulSpeed,fallSpeed,fallCount,climbStart,climbPos0,climbHeight);
  fout.write(uint8_t(jmp));

  fout.write(portal,formerPortal);
  fout.write(diveStart);
  }

void MoveAlgo::tickMobsi(uint64_t dt) {
  if(npc.interactive()->isStaticState())
    return;

  auto dp = npc.animMoveSpeed(dt);
  if(npc.interactive()->isLadder()) {
    auto mat = npc.interactive()->transform();
    mat.project(dp);
    dp -= npc.interactive()->position();
    } else {
    Tempest::Vec3 ret;
    applyRotation(ret,dp);
    dp = Tempest::Vec3(ret.x, 0, ret.z);
    }

  if(npc.interactive()->isTrueDoor(npc)) {
    // some tight-door require collision-detection
    npc.tryMove(dp);
    } else {
    // but chair/bed do not :)
    auto pos = npc.position();
    npc.setPosition(pos+dp);
    }
  setState(Run);
  }

bool MoveAlgo::tryMove(float x, float y, float z) {
  DynamicWorld::CollisionTest out;
  return tryMove(x,y,z,out);
  }

bool MoveAlgo::tryMove(float x,float y,float z, DynamicWorld::CollisionTest& out) {
  return npc.tryMove({x,y,z},out);
  }

bool MoveAlgo::tryMove(const Tempest::Vec3& dp, DynamicWorld::CollisionTest& out) {
  return npc.tryMove(dp,out);
  }

void MoveAlgo::tickJumpup(uint64_t dt) {
  auto pos   = npc.position();
  auto climb = npc.tryJump();

  if(climb.anim==Npc::Anim::JumpHang)
    climbHeight = pos.y;

  if(pos.y<climbHeight) {
    pos.y       += fallSpeed.y*float(dt);
    fallSpeed.y -= gravity*float(dt);

    pos.y = std::min(pos.y,climbHeight);
    if(!npc.tryTranslate(pos))
      climbHeight = pos.y;
    return;
    }

  Tempest::Vec3 p={}, v={0,0,climbMove};
  applyRotation(p,v);
  p = climbPos0;
  p.y = climbHeight;
  npc.tryTranslate(p);

  if(climb.anim==Npc::Anim::JumpHang) {
    startClimb(climb);
    } else {
    setState(InAir);
    clearSpeed();
    }
  }

void MoveAlgo::tickClimb(uint64_t dt) {
  if(npc.bodyStateMasked()!=BS_CLIMB) {
    //NOTE: climb allows npc to violate collision detection, need to readjust
    npc.owner.script().fixNpcPosition(npc, 0, 0);
    setState(Run);

    Tempest::Vec3 p={}, v={0,0,climbMove};
    applyRotation(p,v);
    p += climbPos0;
    p.y = climbHeight;
    if(!npc.tryTranslate(p)) {
      // npc.setPosition(Tempest::Vec3(climbPos0.x,climbHeight,climbPos0.z));
      npc.tryTranslate(Tempest::Vec3(climbPos0.x,climbHeight,climbPos0.z));
      npc.tryTranslate(p);
      }
    clearSpeed();
    return;
    }

  auto dp  = animMoveSpeed(dt);
  auto pos = npc.position();

  if(pos.y<climbHeight) {
    pos.y += dp.y;
    pos.y = std::min(pos.y,climbHeight);
    // npc.tryTranslate(pos);
    npc.setPosition(pos);
    }

  pos.x += dp.x;
  pos.z += dp.z;
  // npc.tryTranslate(pos);
  npc.setPosition(pos);
  }

void MoveAlgo::tick(uint64_t dt, MvFlags moveFlg) {
  if(npc.isDown() && (flags==Swim || flags==Dive)) {
    // 'falling' to bottom of the lake
    setState(InAir);
    }

  if(npc.interactive()!=nullptr) {
    tickMobsi(dt);
    return;
    }

  if(!implTick(dt,moveFlg))
    return;

  if(cache.sector!=nullptr && portal!=cache.sector) {
    formerPortal = portal;
    portal       = cache.sector;
    if(npc.isPlayer() && npc.bodyStateMasked()!=BS_SNEAK) {
      auto& w = npc.world();
      w.sendImmediatePerc(npc,npc,npc,PERC_ASSESSENTERROOM);
      }
    }
  }

bool MoveAlgo::implTick(uint64_t dt, MvFlags moveFlg) {
  if(flags==ClimbUp) {
    tickClimb(dt); //fixup: collision
    return true;
    }
  if(flags==JumpUp) {
    tickJumpup(dt);
    return true;
    }

  const auto state = flags;
  const bool dead  = npc.isDead();
  const bool swim  = (state==Swim);
  const bool dive  = (state==Dive);
  const bool grav  = (state==InAir || state==Falling || state==JumpUp);
  const auto bs    = npc.bodyStateMasked();
  const auto pos0  = npc.position();
  const auto dp    = (!grav && state!=Slide) ? npcMoveSpeed(dt,moveFlg) : npcFallSpeed(dt);
  const bool walk  = bool(npc.walkMode() & WalkBit::WM_Walk);

  DynamicWorld::CollisionTest info;
  if(!tryMove(dp,info)) {
    info.preFall = false;
    if(state==Slide) {
      onGravityFailed(info,dt);
      setState(InAir);
      }
    else if(grav) {
      onGravityFailed(info,dt);
      }
    else if(swim) {
      // Khorinis port hack
      /*
      for(int i=0; i<=50; i+=10) {
        if(tryMove(Tempest::Vec3(dp.x,dp.y+float(i),dp.z), info))
          break;
        if(i==50) {
          onMoveFailed(dp,info,dt);
          return false;
          }
        }
      */
      onMoveFailed(dp,info,dt);
      return false;
      }
    else {
      onMoveFailed(dp,info,dt);
      return false;
      }
    }

  if(grav) {
    fallSpeed.y -= gravity*float(dt);
    }

  //if(dp==Tempest::Vec3() && !grav && state!=Jump && state!=Slide)
  //  return false;

  const auto  pos            = npc.position();
  const float stickThreshold = grav ? 0.f : stepHeight();
  const float chest          = waterDepthChest();
  const float knee           = waterDepthKnee();

  bool  gValid  = false;
  auto  ground  = dropRay (pos, gValid);
  auto  water   = waterRay(pos);
  float dY      = pos.y-ground;

  if(canFlyOverWater() && ground<water) {
    dY      = pos.y-water;
    // onGound = false;
    //NOTE: should disable slide
    }
  else if(std::isfinite(water) && ground+chest<water && npc.hasSwimAnimations() && !dead) {
    dY      = std::min((pos.y+chest)-water, stickThreshold);
    }

  if(dp==Tempest::Vec3() && pos.y==ground && state!=Jump && state!=Slide)
    return false;

  // jump animation (lift off)
  if(bs==BS_JUMP && state!=InAir && state!=Jump && state!=JumpUp) {
    setState(Jump);
    return true;
    }

  // jump animation
  if(state==Jump) {
    if(npc.isJumpAnim()) {
      fallSpeed += dp;
      fallCount += float(dt);
      } else {
      setState(InAir);
      }
    return true;
    }

  if(state==JumpUp) {
    const auto climb = npc.tryJump();
    if(climb.anim==Npc::Anim::JumpHang) {
      clearSpeed();
      startClimb(climb);
      return true;
      }
    //if(pos.y>=climbHeight) {
    //  }
    }

  // blood-fly over water
  if(canFlyOverWater() && !dead && ground<water) {
    setState(Run);
    clearSpeed();
    npc.tryTranslate(Tempest::Vec3(pos.x, water, pos.z));
    return true;
    }

  // water interaction
  if(!canFlyOverWater() && !dead && ground<water) {
    if(swim) {
      // note need to be carefull about non-planar water patches, near waterfalls
      const float wpos = std::max(water-chest, ground);
      npc.tryTranslate(Tempest::Vec3(pos.x, wpos, pos.z));
      }

    const float gpos = std::max(npc.position().y, ground);
    if(gpos + 3.f*chest <= water) {
      // underwater walk bug-like case: can switch to dive here
      // setState(Dive);
      }
    else if(gpos + chest <= water+0.01f) {
      if(state!=Swim && state!=Dive) {
        const bool splash = grav || fallSpeed.quadLength() >= 1.f;
        setState(Swim);
        if(splash)
          emitWaterSplash(water);
        if(!npc.hasSwimAnimations())
          npc.takeDrownDamage();
        clearSpeed();
        return true;
        }
      }
    else if(gpos + knee <= water && state!=InAir && state!=Slide && state!=Dive) {
      // swimming toward cliff-slide
      if(swim && testSlide(pos,info)) {
        npc.setPosition(pos0);
        onMoveFailed(dp,info,dt);
        return false;
        }
      if(state==Swim) {
        setState(Run);
        }
      }
    else if(state==InWater || state==Swim) {
      setState(Run);
      }
    }

  if(swim) {
    if(dead || !std::isfinite(water)) {
      setState(Run);
      }
    return true;
    }

  if(dive) {
    if(pos.y+chest > water && std::isfinite(water)) {
      npc.tryTranslate(Tempest::Vec3(pos.x, water-chest, pos.z));
      if(npc.world().tickCount()-diveStart>2000) {
        setState(Swim);
        }
      }
    return true;
    }

  // above ground/void
  if(!gValid || (pos.y>ground && dY >= stickThreshold && state!=InWater)) {
    if(!gValid && swim) {
      // sea monster condition?
      }
    if(walk || swim) {
      npc.setPosition(pos0);

      info.normal  = dp;
      info.preFall = true;
      onMoveFailed(dp,info,dt);
      return false;
      }
    if(!swim && !dive && !dead) {
      // fall animations
      const float h0 = falldownHeight();

      float fallTime = fallSpeed.y/gravity;
      float height   = 0.5f*std::abs(gravity)*fallTime*fallTime;

      if(height>h0) {
        npc.setAnim(AnimationSolver::FallDeep);
        npc.setAnimRotate(0);
        setState(Falling);
        }
      else if(fallSpeed.y<-0.3f && bs!=BS_JUMP && bs!=BS_FALL) {
        npc.setAnim(AnimationSolver::Fall);
        npc.setAnimRotate(0);
        setState(InAir);
        }
      else if(state==InWater) {
        npc.setAnimRotate(0);
        setState(Swim);
        }
      else {
        npc.setAnimRotate(0);
        setState(InAir);
        }
      }
    return true;
    }

  if(state==Slide && !npc.isDown()) {
    if(!testSlide(pos,info)) {
      setState(Run);
      return true;
      }
    const auto norm    = normalRay(pos);
    const auto tangent = Tempest::Vec3::crossProduct(norm, Tempest::Vec3(0,1,0));
    const auto slide   = Tempest::Vec3::crossProduct(norm, tangent);

    fallSpeed += slide*float(dt)*gravity;
    //fallCount  = 1;
    if(gValid && std::abs(dY)<stickThreshold) {
      // step up
      npc.tryMove(Tempest::Vec3(0,-dY,0));
      }

    npc.setAnimRotate(0);
    if(slideDir())
      npc.setAnim(AnimationSolver::SlideA); else
      npc.setAnim(AnimationSolver::SlideB);
    return true;
    }

  // no longer in air - ground code
  if(state==InAir || state==Falling) {
    // attach to ground
    npc.takeFallDamage(fallSpeed);
    clearSpeed();
    setState(Run);
    }

  if(ground+chest < water && !npc.hasSwimAnimations()) {
    // no swim animations
    npc.setPosition(pos0);
    DynamicWorld::CollisionTest info;
    info.preFall = true;
    onMoveFailed(dp,info,dt);
    return false;
    }

  if(testSlide(pos,info)) {
    if(state==InWater || state==Swim) {
      npc.setPosition(pos0);
      return false;
      }
    if(ground>=pos.y) {
      // same as wall
      npc.setPosition(pos0);
      info.preFall = false;
      onMoveFailed(dp,info,dt);
      return false;
      }
    if(walk) {
      npc.setPosition(pos0);

      info.normal  = dp;
      info.preFall = true;
      onMoveFailed(dp,info,dt);
      return false;
      }
    fallSpeed = Tempest::Vec3();
    fallCount = 0;
    setState(Slide);
    return true;
    }

  if(gValid && dY <= stickThreshold) {
    const float gpos = std::max(npc.position().y, ground);
    if(gpos + knee <= water) {
      setState(InWater);
      } else {
      setState(Run);
      }
    if(ground==pos.y)
      return true;
    if(ground<=pos.y) {
      // step up
      // npc.setPosition(adjPos);
      npc.tryMove(Tempest::Vec3(0,-dY,0));
      return true;
      }
    if(ground>=pos.y) {
      // inside ground
      // npc.setPosition(adjPos);
      npc.tryMove(Tempest::Vec3(0,-dY,0));
      return true;
      }
    }

  // something went wrong - back to origin then
  // npc.setPosition(pos0);
  return true;
  }

void MoveAlgo::clearSpeed() {
  fallSpeed.x = 0;
  fallSpeed.y = 0;
  fallSpeed.z = 0;
  fallCount   = 0;
  }

void MoveAlgo::accessDamFly(float dx, float dz) {
  if(flags==0) {
    float len = std::sqrt(dx*dx+dz*dz);
    auto  vec = Tempest::Vec3(dx,len*0.5f,dz);
    vec = vec/vec.length();

    fallSpeed = vec*1.f;
    fallCount = 0;
    setState(InAir);
    }
  }

void MoveAlgo::applyRotation(Tempest::Vec3& out, const Tempest::Vec3& dpos) const {
  float mul = mulSpeed;
  if(isDive()) {
    float rot = -npc.rotationYRad();
    float s   = std::sin(rot), c = std::cos(rot);

    out.y = -dpos.length()*s;
    mul   = c*mulSpeed;
    } else {
    out.y = dpos.y;
    }
  float rot = npc.rotationRad();
  applyRotation(out,dpos,rot);
  out.x *= -mul;
  out.z *= -mul;
  }

void MoveAlgo::applyRotation(Tempest::Vec3& out, const Tempest::Vec3& dpos, float rot) const {
  float s   = std::sin(rot), c = std::cos(rot);
  out.x = (dpos.x*c-dpos.z*s);
  out.z = (dpos.x*s+dpos.z*c);
  }

Tempest::Vec3 MoveAlgo::animMoveSpeed(uint64_t dt) const {
  auto dp = npc.animMoveSpeed(dt);
  Tempest::Vec3 ret;
  applyRotation(ret,dp);
  return ret;
  }

Tempest::Vec3 MoveAlgo::npcMoveSpeed(uint64_t dt, MvFlags moveFlg) {
  Tempest::Vec3 dp = animMoveSpeed(dt);
  if(!npc.isFlyAnim())
    dp.y = 0.f;

  if(moveFlg&FaiMove) {
    if(npc.currentTarget!=nullptr && !npc.isPlayer() && !npc.currentTarget->isDown()) {
      return go2NpcMoveSpeed(dp,*npc.currentTarget);
      }
    }

  if(NoFlag==(moveFlg&WaitMove)) {
    if(npc.go2.npc)
      return go2NpcMoveSpeed(dp,*npc.go2.npc);

    if(npc.go2.wp)
      return go2WpMoveSpeed(dp,npc.go2.wp->position());

    if(npc.go2.flag!=Npc::GT_No)
      return go2WpMoveSpeed(dp,npc.go2.pos);
    }

  return dp;
  }

Tempest::Vec3 MoveAlgo::npcFallSpeed(uint64_t dt) {
  // falling
  if(0.f<fallCount) {
    fallSpeed/=fallCount;
    fallCount = 0.f;
    }
  return fallSpeed*float(dt);
  }

Tempest::Vec3 MoveAlgo::go2NpcMoveSpeed(const Tempest::Vec3& dp,const Npc& tg) {
  return go2WpMoveSpeed(dp,tg.position());
  }

Tempest::Vec3 MoveAlgo::go2WpMoveSpeed(Tempest::Vec3 dp, const Tempest::Vec3& to) {
  auto  d    = to-npc.position();
  float qLen = (d.x*d.x+d.z*d.z);

  const float qSpeed = dp.x*dp.x+dp.z*dp.z;
  if(qSpeed>0.01f){
    float k = std::sqrt(std::min(qLen,qSpeed)/qSpeed);
    dp.x*=k;
    dp.z*=k;
    }
  return dp;
  }

bool MoveAlgo::testSlide(const Tempest::Vec3& pos, DynamicWorld::CollisionTest& out) const {
  // check ground
  const auto  norm       = normalRay(pos);
  const float slideBegin = std::min(slideAngle(), 1.f);
  const float slideEnd   = slideAngle2();

  out.normal = norm;

  if(!(slideEnd<norm.y && norm.y<slideBegin)) {
    return false;
    }

  const auto tangent = Tempest::Vec3::crossProduct(norm, Tempest::Vec3(0,1,0));
  const auto slide   = Tempest::Vec3::crossProduct(norm, tangent);

  if(!npc.testMove(pos+slide*gravity*0.15f))
    return false;
  return true;
  }

float MoveAlgo::stepHeight() const {
  auto gl = npc.guild();
  auto v  = float(npc.world().script().guildVal().step_height[gl]);
  if(v>0.f)
    return v;
  return 50;
  }

float MoveAlgo::slideAngle() const {
  auto  gl = npc.guild();
  float k  = float(M_PI)/180.f;
  return std::sin((90.f-float(npc.world().script().guildVal().slide_angle[gl]))*k);
  }

float MoveAlgo::slideAngle2() const {
  auto  gl = npc.guild();
  float k  = float(M_PI)/180.f;
  return std::sin(float(npc.world().script().guildVal().slide_angle2[gl])*k);
  }

float MoveAlgo::waterDepthKnee() const {
  auto gl = npc.guild();
  return float(npc.world().script().guildVal().water_depth_knee[gl]);
  }

float MoveAlgo::waterDepthChest() const {
  auto gl = npc.guild();
  return float(npc.world().script().guildVal().water_depth_chest[gl]);
  }

float MoveAlgo::falldownHeight() const {
  auto gl = npc.guild();
  return float(npc.world().script().guildVal().falldown_height[gl]);
  }

bool MoveAlgo::canFlyOverWater() const {
  auto  gl = npc.guild();
  auto& g  = npc.world().script().guildVal();
  return g.water_depth_chest[gl]==flyOverWaterHint &&
         g.water_depth_knee [gl]==flyOverWaterHint;
  }

bool MoveAlgo::canFallByGravity() const {
  return falldownHeight()!=9999;
  }

bool MoveAlgo::checkLastBounce() const {
  uint64_t ticks = npc.world().tickCount();
  return lastBounce+1000<ticks;
  }

void MoveAlgo::emitWaterSplash(float y) {
  auto& world = npc.world();

  auto pos = npc.position();
  pos.y = y;

  Tempest::Matrix4x4 at;
  at.identity();
  at.translate(pos);

  Effect e(PfxEmitter(world,"PFX_WATERSPLASH"),"");
  e.setObjMatrix(at);
  e.setActive(true);
  world.runEffect(std::move(e));

  npc.emitSoundEffect("CS_INTRO_WATERSPLASH.WAV",2500,false);
  }

int32_t MoveAlgo::diveTime() const {
  if(!isDive())
    return 0;
  return int32_t(npc.world().tickCount() - diveStart);
  }

bool MoveAlgo::isClose(const Npc& npc, const Npc& p, float dist) {
  float len = npc.qDistTo(p);
  return (len<dist*dist);
  }

bool MoveAlgo::isClose(const Npc& npc, const WayPoint& p) {
  float dist = closeToPointThreshold;
  if(p.useCounter()>1)
    dist += 100; // needed for some peasants on Onars farm
  return isClose(npc,p,dist);
  }

bool MoveAlgo::isClose(const Npc& npc, const WayPoint& p, float dist) {
  auto  dp  = p.groundPos - npc.position();
  float len = dp.quadLength();
  return (len<dist*dist);
  }

bool MoveAlgo::isClose(const Npc& npc, const Tempest::Vec3& p, float dist) {
  auto  dp  = (p - npc.centerPosition());
  float len = dp.quadLength();
  // NOTE: extra check fo vertical only distances:
  //       some repair planks are too high (~5 meters) in the air
  return (len<dist*dist) || Tempest::Vec2(dp.x,dp.z).quadLength() < 1.f;
  }

bool MoveAlgo::startClimb(JumpStatus jump) {
  auto sq = npc.setAnimAngGet(jump.anim);
  if(sq==nullptr)
    return false;

  climbStart  = npc.world().tickCount();
  climbPos0   = npc.position();
  climbHeight = jump.height;

  const float dHeight = (jump.height-climbPos0.y);

  fallSpeed.x = 0.f;
  fallSpeed.y = dHeight/sq->totalTime();
  fallSpeed.z = 0.f;
  fallCount   = 0.f;

  if(jump.anim==Npc::Anim::JumpUp && dHeight>0.f){
    setState(JumpUp);
    float t = std::sqrt(2.f*dHeight/gravity);
    fallSpeed.y = gravity*t;
    }
  else if(jump.anim==Npc::Anim::JumpUpMid ||
          jump.anim==Npc::Anim::JumpUpLow) {
    setState(ClimbUp);
    }
  else if(isJumpUp() && jump.anim==Npc::Anim::JumpHang) {
    setState(ClimbUp);
    }
  else {
    return false;
    }
  return true;
  }

void MoveAlgo::startDive() {
  if(!isSwim())
    return;

  if(npc.world().tickCount()-diveStart>1000) {
    setState(Dive);

    auto  pos   = npc.position();
    float pY    = pos.y;
    float chest = canFlyOverWater() ? 0 : waterDepthChest();
    auto  water = waterRay(pos);
    tryMove(0,water-chest-pY,0);
    }
  }

bool MoveAlgo::isFalling() const {
  return flags==Falling;
  }

bool MoveAlgo::isSlide() const {
  return flags==Slide;
  }

bool MoveAlgo::isInAir() const {
  return flags==InAir;
  }

bool MoveAlgo::isJumpUp() const {
  return flags==JumpUp;
  }

bool MoveAlgo::isClimb() const {
  return flags==ClimbUp;
  }

bool MoveAlgo::isInWater() const {
  return flags==InWater;
  }

bool MoveAlgo::isSwim() const {
  return flags==Swim;
  }

bool MoveAlgo::isDive() const {
  return flags==Dive;
  }

void MoveAlgo::setState(State f) {
  if(f==flags)
    return;

#ifndef NDEBUG
  assertStateChange(f);
#endif

  if((f==Swim) && !(flags==Swim)) {
    auto ws = npc.weaponState();
    npc.setAnim(Npc::Anim::NoAnim);
    if(ws!=WeaponState::NoWeapon && ws!=WeaponState::Fist)
      npc.closeWeapon(true);
    npc.dropTorch(true);
    }

  if((f==Dive) && !(flags==Dive)) {
    npc.setDirectionY(-40);
    }
  if((f==Dive) != (flags==Dive)) {
    diveStart = npc.world().tickCount();
    }

  // handle fly-speed here?

  flags = f;
  }

void MoveAlgo::assertStateChange(State f) {
  // assert possible transitions
  switch(flags) {
    case Run:
      assert(f!=Falling);
      break;
    case InAir:
      assert(f==Run || f==Falling || f==InWater || f==Swim || f==Dive);
      break;
    case Falling:
      assert(f==Run || f==InAir || f==InWater || f==Swim || f==Dive);
      break;
    case Slide:
      break;
    case Jump:
      assert(f==Run || f==InAir || f==Falling || f==Swim || f==Dive);
      break;
    case JumpUp:
      assert(f==Run || f==InAir || f==ClimbUp);
      break;
    case ClimbUp:
      assert(f==Run);
      break;
    case InWater:
      assert(f==Run || f==Slide || f==JumpUp || f==Swim || f==Dive);
      break;
    case Swim:
      assert(f==Run || f==InAir || f==InWater || f==Dive);
      break;
    case Dive:
      assert(f==InAir || f==Swim || f==InWater);
      break;
    }
  }

bool MoveAlgo::slideDir() const {
  float a = std::atan2(fallSpeed.x,fallSpeed.z)+float(M_PI/2);
  float b = npc.rotationRad();

  auto s = std::sin(a-b);
  return s>0;
  }

bool MoveAlgo::testMoveDirection(const Tempest::Vec3& dp, const Tempest::Vec3& dir) const {
  Tempest::Vec3 tr, tr2;
  applyRotation(tr,dir);
  tr2 = Tempest::Vec3::normalize(dp);
  return Tempest::Vec3::dotProduct(tr,tr2)>0.8f;
  }

bool MoveAlgo::isForward(const Tempest::Vec3& dp) const {
  return testMoveDirection(dp,{0,0, 1});
  }

bool MoveAlgo::isBackward(const Tempest::Vec3& dp) const {
  return testMoveDirection(dp,{0,0,-1});
  }

void MoveAlgo::onMoveFailed(const Tempest::Vec3& dp, const DynamicWorld::CollisionTest& info, uint64_t dt) {
  static float threshold = 0.4f;
  static float speed     = 360.f;

  if(dp==Tempest::Vec3())
    return;

  const auto  ortho   = Tempest::Vec3::crossProduct(Tempest::Vec3::normalize(dp),Tempest::Vec3(0,1,0));
  const float stp     = speed*float(dt)/1000.f;
  const float val     = Tempest::Vec3::dotProduct(ortho,info.normal);
  const bool  forward = isForward(dp);

  if(info.vob!=nullptr && forward && npc.interactive()!=info.vob) {
    npc.setDetectedMob(info.vob);
    npc.perceptionProcess(npc,nullptr,0,PERC_MOVEMOB);
    if(npc.moveHint()==Npc::GT_No)
      return;
    }

  if(forward && !info.preFall && npc.currentTarget==info.npc && npc.bodyStateMasked()==BS_HIT)
    return;

  if(npc.processPolicy()!=NpcProcessPolicy::Player)
    lastBounce = npc.world().tickCount();

  if(std::abs(val)>=threshold && !info.preFall && checkLastBounce()) {
    // emulate bouncing behaviour of original game
    Tempest::Vec3 corr;
    for(int i=5; i<=35; i+=5) {
      for(float angle:{float(i),-float(i)}) {
        applyRotation(corr,dp,float(angle*M_PI)/180.f);
        if(npc.testMove(corr)) {
          if(forward)
            npc.setDirection(npc.rotation()+angle);
          return;
          }
        }
      }
    }

  if(!forward)
    return;

  if(!info.preFall && npc.isAttackAnim())
    return;

  npc.setAnimRotate(0);
  if(val<-threshold) {
    npc.setDirection(npc.rotation()-stp);
    }
  else if(val>threshold) {
    npc.setDirection(npc.rotation()+stp);
    }
  else switch(npc.moveHint()) {
    case Npc::GT_No:
      npc.setAnim(Npc::Anim::Idle);
      break;
    case Npc::GT_NextFp:
      npc.clearGoTo();
      break;
    case Npc::GT_Item:
      npc.setDirection(npc.rotation()+stp);
      break;
    case Npc::GT_EnemyA:
    case Npc::GT_EnemyG:
    case Npc::GT_Way:
    case Npc::GT_Point: {
      if(info.npc!=nullptr || info.preFall) {
        npc.setDirection(npc.rotation()+stp);
        } else {
        auto jc = npc.tryJump();
        if(jc.noClimb)
          npc.setDirection(npc.rotation()+stp); else
        if(!npc.startClimb(jc))
          npc.setDirection(npc.rotation()+stp);
        }
      break;
      }
    case Npc::GT_Flee:
      npc.setDirection(npc.rotation()+stp);
      break;
    }
  }

void MoveAlgo::onGravityFailed(const DynamicWorld::CollisionTest& info, uint64_t dt) {
  auto        norm      = info.normal;
  const float normXZ    = std::sqrt(norm.x*norm.x+norm.z*norm.z);
  const float normXZInv = std::sqrt(1.f - normXZ*normXZ);

  if(normXZ>0.001f && normXZ<std::fabs(norm.y) && normXZInv>0.001f) {
    norm.x = normXZInv*norm.x/normXZ;
    norm.z = normXZInv*norm.z/normXZ;
    norm.y = normXZ   *norm.y/normXZInv;
    }

  if(Tempest::Vec3::dotProduct(fallSpeed,norm)<0.f || fallCount>0) {
    float len  = fallSpeed.length()/std::max(1.f,fallCount);
    if(isInAir() && Tempest::Vec2::dotProduct({fallSpeed.x, fallSpeed.z}, {norm.x, norm.z})<0.f) {
      float lx = Tempest::Vec2({fallSpeed.x, fallSpeed.z}).length();
      lx *= 0.25f;
      fallSpeed.x = norm.x*lx;
      fallSpeed.z = norm.z*lx;
      //fallSpeed   = Tempest::Vec3::normalize(fallSpeed)*len;
      } else {
      len *= 0.25f;
      len = std::max(len, 0.1f);
      fallSpeed   = Tempest::Vec3::normalize(fallSpeed+norm)*len;
      }
    fallCount  = 0;
    } else {
    fallSpeed += 10.f*norm*gravity*float(dt);
    }
  }

float MoveAlgo::waterRay(const Tempest::Vec3& pos) const {
  if(std::fabs(cacheW.x-pos.x)>eps || std::fabs(cacheW.y-pos.y)>eps || std::fabs(cacheW.z-pos.z)>eps) {
    const float threshold = -0.1f; //npc.visual.pose().translateY()+1.f;
    const auto  spos      = Tempest::Vec3(pos.x, pos.y+threshold, pos.z);
    static_cast<DynamicWorld::RayWaterResult&>(cacheW) = npc.world().physic()->waterRay(spos);
    cacheW.x = pos.x;
    cacheW.y = pos.y;
    cacheW.z = pos.z;
    }
  return cacheW.wdepth;
  }

void MoveAlgo::rayMain(const Tempest::Vec3& pos) const {
  if(std::fabs(cache.x-pos.x)>eps || std::fabs(cache.y-pos.y)>eps || std::fabs(cache.z-pos.z)>eps) {
    float threshold = npc.visual.pose().translateY()+1.f;
    float dy        = threshold+100;  // 1 meter extra offset
    if(fallSpeed.y<0 || false)
      dy = 0; // whole world
    const auto spos = Tempest::Vec3(pos.x, pos.y+threshold, pos.z);
    static_cast<DynamicWorld::RayLandResult&>(cache) = npc.world().physic()->landRay(spos,dy);
    cache.x = pos.x;
    cache.y = pos.y;
    cache.z = pos.z;
    }
  }

float MoveAlgo::dropRay(const Tempest::Vec3& pos, bool &hasCol) const {
  rayMain(pos);
  hasCol = cache.hasCol;
  return cache.v.y;
  }

Tempest::Vec3 MoveAlgo::normalRay(const Tempest::Vec3& pos) const {
  rayMain(pos);
  return cache.n;
  }

zenkit::MaterialGroup MoveAlgo::groundMaterial() const {
  const float stp = stepHeight();
  if(cacheW.wdepth+stp>cache.v.y)
    return zenkit::MaterialGroup::WATER;
  return cache.mat;
  }

Tempest::Vec3 MoveAlgo::groundNormal() const {
  return cache.n;
  }

std::string_view MoveAlgo::portalName() {
  return portal;
  }

std::string_view MoveAlgo::formerPortalName() {
  return formerPortal;
  }

