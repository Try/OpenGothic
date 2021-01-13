#include "movealgo.h"

#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/world.h"
#include "serialize.h"

const float   MoveAlgo::closeToPointThreshold = 50;
const float   MoveAlgo::climbMove             = 55;
const float   MoveAlgo::gravity               = DynamicWorld::gravity;
const float   MoveAlgo::eps                   = 2.f;   // 2-santimeters
const float   MoveAlgo::epsAni                = 0.25f; // 25-millimeters
const int32_t MoveAlgo::flyOverWaterHint      = 999999;

MoveAlgo::MoveAlgo(Npc& unit)
  :npc(unit) {
  }

void MoveAlgo::load(Serialize &fin) {
  if(fin.version()<7){
    uint8_t bug[3];
    fin.read(reinterpret_cast<uint32_t&>(flags));
    fin.read(mulSpeed,bug,fallCount,climbStart,bug,climbHeight);
    fin.read(reinterpret_cast<uint8_t&>(jmp));
    return;
    }
  fin.read(reinterpret_cast<uint32_t&>(flags));
  fin.read(mulSpeed,fallSpeed,fallCount,climbStart,climbPos0,climbHeight);
  fin.read(reinterpret_cast<uint8_t&>(jmp));
  if(fin.version()>=14) {
    uint8_t str[128]={};
    fin.read(str);
    if(str[0]!=0)
      cache.sector = npc.world().physic()->validateSectorName(reinterpret_cast<char*>(str)); else
      cache.sector = nullptr;
    }
  if(fin.version()>17) {
    fin.read(diveStart);
    }
  }

void MoveAlgo::save(Serialize &fout) const {
  fout.write(uint32_t(flags));
  fout.write(mulSpeed,fallSpeed,fallCount,climbStart,climbPos0,climbHeight);
  fout.write(uint8_t(jmp));

  const auto len = cache.sector==nullptr ? 0 : std::strlen(cache.sector);
  uint8_t str[128]={};
  if(len>0 && len<std::extent<decltype(str)>::value)
    std::strcpy(reinterpret_cast<char*>(str),cache.sector);
  fout.write(str);
  fout.write(diveStart);
  }

void MoveAlgo::tickMobsi(uint64_t dt) {
  if(npc.interactive()->isStaticState())
    return;

  auto dp  = animMoveSpeed(dt);
  auto pos = npc.position();
  pos.x+=dp.x;
  //pos.x+=dp.y;
  pos.z+=dp.z;
  npc.setPosition(pos);
  setAsSlide(false);
  setInAir  (false);
  }

bool MoveAlgo::tryMove(float x,float y,float z) {
  if(flags==NoFlags && std::fabs(x)<epsAni && std::fabs(y)<epsAni && std::fabs(z)<epsAni) {
    skipMove = Tempest::Vec3(x,y,z);
    return true;
    }
  skipMove = Tempest::Vec3();
  return npc.tryMove({x,y,z});
  }

bool MoveAlgo::tickSlide(uint64_t dt) {
  float fallThreshold = stepHeight();
  auto  pos           = npc.position();

  auto  norm   = normalRay(pos.x,pos.y+fallThreshold,pos.z);
  // check ground
  float pY     = pos.y;
  bool  valid  = false;
  auto  ground = dropRay (pos.x, pos.y+fallThreshold, pos.z, valid);
  auto  water  = waterRay(pos.x, pos.y, pos.z);
  float dY     = pY-ground;

  if(ground+waterDepthChest()<water) {
    setInAir(true);
    setAsSlide(false);
    return true;
    }
  if(dY>fallThreshold*2) {
    setInAir  (true);
    setAsSlide(false);
    return false;
    }

  const float lnorm    = std::sqrt(norm.x*norm.x+norm.z*norm.z);
  const float lnormInv = std::sqrt(1.f - lnorm*lnorm);
  if(lnorm<0.01f || norm.y>=1.f || !testSlide(pos.x,pos.y+fallThreshold,pos.z)) {
    setAsSlide(false);
    return false;
    }

  const float speed = float(dt)*gravity;

  norm.x = lnormInv*norm.x/lnorm;
  norm.z = lnormInv*norm.z/lnorm;
  norm.y = lnorm   *norm.y/lnormInv;

  float k = 0.8f;
  fallSpeed.x += +speed*norm.x*k;
  fallSpeed.y += -speed*norm.y*k;
  fallSpeed.z += +speed*norm.z*k;

  auto dp   = fallSpeed*float(dt);
  //auto pos0 = npc.position();
  if(!npc.tryMove(dp))
    npc.tryMove(Tempest::Vec3(dp.x,0,dp.y));
  //auto dpos = (npc.position()-pos0)/float(dt);
  //fallSpeed.y = dpos.y;

  if(slideDir())
    npc.setAnim(AnimationSolver::SlideA); else
    npc.setAnim(AnimationSolver::SlideB);

  setInAir  (false);
  setAsSlide(true);
  return true;
  }

void MoveAlgo::tickGravity(uint64_t dt) {
  float fallThreshold = stepHeight();
  // falling
  if(0.f<fallCount) {
    fallSpeed/=fallCount;
    fallCount=-1.f;
    }
  fallSpeed.y   -= gravity*float(dt);

  // check ground
  auto  pos      = npc.position();
  float pY       = pos.y;
  float chest    = canFlyOverWater() ? 0 : waterDepthChest();
  bool  valid    = false;
  auto  ground   = dropRay (pos.x, pos.y+fallThreshold, pos.z, valid);
  auto  water    = waterRay(pos.x, pos.y, pos.z);
  float fallStop = std::max(water-chest,ground);

  auto dp        = fallSpeed*float(dt);

  if(pY+dp.y>fallStop || dp.y>0) {
    // continue falling
    if(!tryMove(dp.x,dp.y,dp.z)) {
      fallSpeed.y=0.f;
      if((std::fabs(dp.x)<0.001f && std::fabs(dp.z)<0.001f) || !tryMove(dp.x,0.f,dp.z)) {
        // attach to ground
        setInAir(false);
        if(!npc.isDead())
          npc.setAnim(AnimationSolver::Idle);
        return;
        }
      setInAir(false);
      }
    if(fallSpeed.y<-1.5f && !npc.isDead())
      npc.setAnim(AnimationSolver::FallDeep); else
    if(fallSpeed.y<-0.3f && !npc.isDead())
      npc.setAnim(AnimationSolver::Fall);
    } else {
    if(ground+chest<water && !npc.isDead()) {
      // attach to water
      tryMove(0.f,water-pY,0.f);
      clearSpeed();
      setInAir(false);
      if(!canFlyOverWater()) {
        setInWater(true);
        setAsSwim(true);
        npc.setAnim(AnimationSolver::Idle);
        }
      } else {
      // attach to ground
      tryMove(0.f,ground-pY,0.f);
      takeFallDamage();
      clearSpeed();
      setInAir(false);
      }
    }
  }

void MoveAlgo::tickJumpup(uint64_t dt) {
  auto pos = npc.position();
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

  auto climb = npc.tryJump();
  if(climb.anim==Npc::Anim::JumpHang) {
    startClimb(climb);
    } else {
    setAsClimb(false);
    setAsJumpup(false);
    clearSpeed();
    }
  }

void MoveAlgo::tickClimb(uint64_t dt) {
  if(npc.bodyStateMasked()!=BS_CLIMB) {
    setAsClimb(false);
    setInAir  (false);

    Tempest::Vec3 p={}, v={0,0,climbMove};
    applyRotation(p,v);
    p += climbPos0;
    p.y = climbHeight;
    if(!npc.tryTranslate(p)) {
      npc.setPosition(Tempest::Vec3(climbPos0.x,climbHeight,climbPos0.z));
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
    npc.tryTranslate(pos);
    }

  pos.x += dp.x;
  pos.z += dp.z;
  npc.tryTranslate(pos);

  setAsSlide(false);
  setInAir  (false);
  }

void MoveAlgo::tickSwim(uint64_t dt) {
  auto  dp            = npcMoveSpeed(dt,MvFlags::NoFlag);
  auto  pos           = npc.position();
  float pY            = pos.y;
  float fallThreshold = stepHeight();
  auto  chest         = waterDepthChest();

  bool  valid  = false;
  auto  ground = dropRay (pos.x+dp.x, pos.y+dp.y+fallThreshold, pos.z+dp.z, valid);
  auto  water  = waterRay(pos.x+dp.x, pos.y+dp.y-chest,         pos.z+dp.z);

  if(npc.isDead()){
    setAsSwim(false);
    setAsDive(false);
    return;
    }

  if(ground+chest>=water) {
    if(testSlide(pos.x+dp.x, pos.y+dp.y+fallThreshold, pos.z+dp.z))
      return;
    setAsSwim(false);
    setAsDive(false);
    tryMove(dp.x,ground-pY,dp.z);
    return;
    }

  if(isDive() && pos.y>water) {
    if(npc.world().tickCount()-diveStart>1000)
      setAsDive(false);
    return;
    }

  // swim on top of water
  if(!isDive())
    tryMove(dp.x,water-pY,dp.z); else
    tryMove(dp.x,dp.y,dp.z);
  }

void MoveAlgo::tick(uint64_t dt, MvFlags moveFlg) {
  if(npc.interactive()!=nullptr)
    return tickMobsi(dt);

  if(isClimb())
    return tickClimb(dt);

  if(isJumpup())
    return tickJumpup(dt);

  if(isSwim())
    return tickSwim(dt);

  if(isInAir() && !npc.isJumpAnim()) {
    return tickGravity(dt);
    }

  if(isSlide()){
    if(tickSlide(dt))
      return;
    if(isInAir())
      return;
    }

  auto  dp            = skipMove+npcMoveSpeed(dt,moveFlg);
  auto  pos           = npc.position();
  float pY            = pos.y;
  float fallThreshold = stepHeight();

  // moving NPC, by animation
  bool  valid   = false;
  auto  ground  = dropRay (pos.x+dp.x, pos.y+dp.y+fallThreshold, pos.z+dp.z, valid);
  auto  water   = waterRay(pos.x+dp.x, pos.y+dp.y,               pos.z+dp.z);
  float dY      = pY-ground;
  bool  onGound = true;

  if(!npc.isDead() && !npc.isJumpAnim() && ground+waterDepthChest()<water) {
    setInAir(false);
    setInWater(true);
    setAsSwim(true);
    return;
    }

  if(canFlyOverWater() && ground<water) {
    dY      = 0;
    onGound = false;
    } else {
    if(ground+waterDepthKnee()<water)
      setInWater(true); else
      setInWater(false);
    }

  if(-fallThreshold<dY && npc.isFlyAnim()) {
    // jump animation
    tryMove(dp.x,dp.y,dp.z);
    fallSpeed += dp;
    fallCount += float(dt);
    setInAir  (true);
    setAsSlide(false);
    }
  else if(0.f<=dY && dY<fallThreshold) {
    if(onGound && testSlide(pos.x+dp.x, pos.y+dp.y+fallThreshold, pos.z+dp.z)) {
      tryMove(dp.x,-dY,dp.z);
      setAsSlide(true);
      return;
      }
    // move down the ramp
    if(!tryMove(dp.x,-dY,dp.z)){
      if(!tryMove(dp.x,dp.y,dp.z))
        onMoveFailed();
      }
    setInAir  (false);
    setAsSlide(false);
    }
  else if(-fallThreshold<dY && dY<0.f) {
    if(onGound && testSlide(pos.x+dp.x, pos.y+dp.y+fallThreshold, pos.z+dp.z)) {
      onMoveFailed();
      return;
      }
    // move up the ramp
    if(!tryMove(dp.x,-dY,dp.z))
      onMoveFailed();
    setInAir  (false);
    setAsSlide(false);
    }
  else if(0.f<dY) {
    const bool walk = bool(npc.walkMode()&WalkBit::WM_Walk);
    if((!walk && npc.isPlayer()) ||
       (dY<300.f && !npc.isPlayer()) ||
       isSlide()) {
      // start to fall of cliff
      if(tryMove(dp.x,dp.y,dp.z)){
        fallSpeed.x = 0.3f*dp.x;
        fallSpeed.y = 0.f;
        fallSpeed.z = 0.3f*dp.z;
        fallCount    = float(dt);
        setInAir  (true);
        setAsSlide(false);
        } else {
        tryMove(dp.x,0,dp.z);
        }
      }
    }
  }

void MoveAlgo::clearSpeed() {
  fallSpeed.x = 0;
  fallSpeed.y = 0;
  fallSpeed.z = 0;
  fallCount   =-1.f;
  }

void MoveAlgo::accessDamFly(float dx, float dz) {
  if(flags==0) {
    float len = std::sqrt(dx*dx+dz*dz);
    auto  vec = Tempest::Vec3(dx,len*0.5f,dz);
    vec = vec/vec.manhattanLength();

    fallSpeed = vec*1.f;
    fallCount =-1.f;
    setInAir(true);
    }
  }

void MoveAlgo::applyRotation(Tempest::Vec3& out, const Tempest::Vec3& dpos) const {
  float mul = mulSpeed;
  if((npc.bodyState()&BS_DIVE)==BS_DIVE) {
    float rot = -npc.rotationYRad();
    float s   = std::sin(rot), c = std::cos(rot);

    out.y = -dpos.manhattanLength()*s;
    mul   = c*mulSpeed;
    } else {
    out.y = dpos.y;
    }
  float rot = npc.rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);
  out.x = -mul*(dpos.x*c-dpos.z*s);
  out.z = -mul*(dpos.x*s+dpos.z*c);
  }

Tempest::Vec3 MoveAlgo::animMoveSpeed(uint64_t dt) const {
  auto dp = npc.animMoveSpeed(dt);
  Tempest::Vec3 ret;
  applyRotation(ret,dp);
  return ret;
  }

Tempest::Vec3 MoveAlgo::npcMoveSpeed(uint64_t dt, MvFlags moveFlg) {
  Tempest::Vec3 dp = animMoveSpeed(dt);
  if(!npc.isJumpAnim())
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

bool MoveAlgo::testSlide(float x,float y,float z) const {
  if(isInAir() || npc.isJumpAnim())
    return false;

  auto  norm             = normalRay(x,y,z);
  // check ground
  const float slideBegin = slideAngle();
  const float slideEnd   = slideAngle2();

  if(!(slideEnd<norm.y && norm.y<slideBegin)) {
    return false;
    }
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

bool MoveAlgo::canFlyOverWater() const {
  auto  gl = npc.guild();
  auto& g  = npc.world().script().guildVal();
  return g.water_depth_chest[gl]==flyOverWaterHint &&
         g.water_depth_knee [gl]==flyOverWaterHint;
  }

void MoveAlgo::takeFallDamage() const {
  auto  gl = npc.guild();
  auto& g  = npc.world().script().guildVal();

  float speed       = fallSpeed.y;
  float fallTime    = speed/gravity;
  float height      = 0.5f*std::abs(gravity)*fallTime*fallTime;
  float h0          = float(g.falldown_height[gl]);
  float dmgPerMeter = float(g.falldown_damage[gl]);

  int32_t hp   = npc.attribute(Npc::ATR_HITPOINTS);
  int32_t prot = npc.protection(Npc::PROT_FALL);

  int32_t damage = int32_t(dmgPerMeter*(height-h0)/100.f - float(prot));
  if(damage<=0)
    return;

  if(hp>damage) {
    npc.emitSoundSVM("SVM_%d_AARGH");
    npc.setAnim(Npc::Anim::Fallen);
    }
  npc.changeAttribute(Npc::ATR_HITPOINTS,-damage,false);
  }

int32_t MoveAlgo::diveTime() const {
  if(!isDive())
    return 0;
  return int32_t(npc.world().tickCount() - diveStart);
  }

bool MoveAlgo::isClose(const Tempest::Vec3& w, const WayPoint &p) {
  return isClose(w.x,w.y,w.z,p);
  }

bool MoveAlgo::isClose(float x, float y, float z, const WayPoint &p) {
  return isClose(x,y,z,p,closeToPointThreshold);
  }

bool MoveAlgo::isClose(float x, float /*y*/, float z, const WayPoint &p, float dist) {
  float len = p.qDistTo(x,p.y,z);
  return (len<dist*dist);
  }

bool MoveAlgo::aiGoTo(const Tempest::Vec3& p, float destDist) {
  // use smaller threshold, to avoid edge-looping in script
  auto  dp  = npc.position()-p;
  dp.y = 0;
  float len = dp.quadLength();

  if(len<destDist*destDist)
    return false;
  return true;
  }

bool MoveAlgo::aiGoToTarget(float destDist) {
  auto p = npc.currentTarget;
  if(p==nullptr)
    return false;
  float len = npc.qDistTo(*p);
  if(len<destDist*destDist){
    return false;
    }
  return true;
  }

bool MoveAlgo::startClimb(JumpStatus jump) {
  auto sq = npc.setAnimAngGet(jump.anim,false);
  if(sq==nullptr)
    return false;

  climbStart  = npc.world().tickCount();
  climbPos0   = npc.position();
  climbHeight = jump.height;

  const float dHeight = (jump.height-climbPos0.y);

  fallSpeed.x = 0.f;
  fallSpeed.y = dHeight/sq->totalTime();
  fallSpeed.z = 0.f;
  fallCount   = -1.f;

  if(jump.anim==Npc::Anim::JumpUp && dHeight>0.f){
    setAsJumpup(true);
    setInAir(true);

    float t = std::sqrt(2.f*dHeight/gravity);
    fallSpeed.y = gravity*t;
    }
  else if(jump.anim==Npc::Anim::JumpUpMid ||
          jump.anim==Npc::Anim::JumpUpLow) {
    setAsJumpup(false);
    setAsClimb(true);
    setInAir(true);
    }
  else if(isJumpup() && jump.anim==Npc::Anim::JumpHang) {
    setAsJumpup(false);
    setAsClimb(true);
    setInAir(true);
    }
  else {
    return false;
    }
  return true;
  }

void MoveAlgo::startDive() {
  if(isSwim() && !isDive()) {
    if(npc.world().tickCount()-diveStart>1000) {
      setAsDive(true);
      }
    }
  }

bool MoveAlgo::isFaling() const {
  return flags&Faling;
  }

bool MoveAlgo::isSlide() const {
  return flags&Slide;
  }

bool MoveAlgo::isInAir() const {
  return flags&InAir;
  }

bool MoveAlgo::isJumpup() const {
  return flags&JumpUp;
  }

bool MoveAlgo::isClimb() const {
  return flags&ClimbUp;
  }

bool MoveAlgo::isInWater() const {
  return flags&InWater;
  }

bool MoveAlgo::isSwim() const {
  return flags&Swim;
  }

bool MoveAlgo::isDive() const {
  return flags&Dive;
  }

void MoveAlgo::setInAir(bool f) {
  if(f)
    flags=Flags(flags|InAir); else
    flags=Flags(flags&(~InAir));
  }

void MoveAlgo::setAsJumpup(bool f) {
  if(f)
    flags=Flags(flags|JumpUp); else
    flags=Flags(flags&(~JumpUp));
  }

void MoveAlgo::setAsClimb(bool f) {
  if(f)
    flags=Flags(flags|ClimbUp); else
    flags=Flags(flags&(~ClimbUp));
  }

void MoveAlgo::setAsSlide(bool f) {
  if(f)
    flags=Flags(flags|Slide);  else
    flags=Flags(flags&(~Slide));
  }

void MoveAlgo::setInWater(bool f) {
  if(f)
    flags=Flags(flags|InWater);  else
    flags=Flags(flags&(~InWater));
  }

void MoveAlgo::setAsSwim(bool f) {
  if(f) {
    auto ws = npc.weaponState();
    npc.setAnim(Npc::Anim::NoAnim);
    if(ws!=WeaponState::NoWeapon && ws!=WeaponState::Fist)
      npc.closeWeapon(true);
    }

  if(f)
    flags=Flags(flags|Swim);  else
    flags=Flags(flags&(~Swim));
  }

void MoveAlgo::setAsDive(bool f) {
  if(f==isDive())
    return;
  if(f) {
    npc.setDirectionY(-60);
    diveStart = npc.world().tickCount();
    npc.setWalkMode(npc.walkMode() | WalkBit::WM_Dive);
    } else {
    diveStart = npc.world().tickCount();
    npc.setWalkMode(npc.walkMode() & (~WalkBit::WM_Dive));
    }
  if(f)
    flags=Flags(flags|Dive);  else
    flags=Flags(flags&(~Dive));
  }

bool MoveAlgo::slideDir() const {
  float a = std::atan2(fallSpeed.x,fallSpeed.z)+float(M_PI/2);
  float b = npc.rotationRad();

  auto s = std::sin(a-b);
  return s>0;
  }

void MoveAlgo::onMoveFailed() {
  if(npc.moveHint()==Npc::GT_NextFp){
    npc.clearGoTo();
    }
  }

float MoveAlgo::waterRay(float x, float y, float z) const {
  if(std::fabs(cacheW.x-x)>eps || std::fabs(cacheW.y-y)>eps || std::fabs(cacheW.z-z)>eps) {
    static_cast<DynamicWorld::RayWaterResult&>(cacheW) = npc.world().physic()->waterRay(x,y,z);
    cacheW.x = x;
    cacheW.y = y;
    cacheW.z = z;
    }
  return cacheW.wdepth;
  }

void MoveAlgo::rayMain(float x, float y, float z) const {
  if(std::fabs(cache.x-x)>eps || std::fabs(cache.y-y)>eps || std::fabs(cache.z-z)>eps) {
    auto  prev = cache.sector;
    float dy   = waterDepthChest()+100;  // 1 meter extra offset
    if(fallSpeed.y<0)
      dy = 0; // whole world
    static_cast<DynamicWorld::RayLandResult&>(cache) = npc.world().physic()->landRay(x,y,z,dy);
    cache.x = x;
    cache.y = y;
    cache.z = z;
    if(!cache.hasCol) {
      cache.sector = prev;
      }
    }
  }

float MoveAlgo::dropRay(float x, float y, float z, bool &hasCol) const {
  rayMain(x,y,z);
  hasCol = cache.hasCol;
  return cache.v.y;
  }

Tempest::Vec3 MoveAlgo::normalRay(float x, float y, float z) const {
  rayMain(x,y,z);
  return cache.n;
  }

uint8_t MoveAlgo::groundMaterial() const {
  const float stp = stepHeight();
  if(cacheW.wdepth+stp>cache.v.y)
    return ZenLoad::MaterialGroup::WATER;
  return cache.mat;
  }

Tempest::Vec3 MoveAlgo::groundNormal() const {
  return cache.n;
  }

const char* MoveAlgo::portalName() {
  return cache.sector;
  }

