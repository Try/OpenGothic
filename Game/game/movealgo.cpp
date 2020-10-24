#include "movealgo.h"

#include "serialize.h"
#include "world/world.h"
#include "world/npc.h"

const float   MoveAlgo::closeToPointThreshold = 50;
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
      cache.portalName = npc.world().physic()->validateSectorName(reinterpret_cast<char*>(str)); else
      cache.portalName = nullptr;
    }
  if(fin.version()>17) {
    fin.read(diveStart);
    }
  }

void MoveAlgo::save(Serialize &fout) const {
  fout.write(uint32_t(flags));
  fout.write(mulSpeed,fallSpeed,fallCount,climbStart,climbPos0,climbHeight);
  fout.write(uint8_t(jmp));

  const auto len = cache.portalName==nullptr ? 0 : std::strlen(cache.portalName);
  uint8_t str[128]={};
  if(len>0 && len<std::extent<decltype(str)>::value)
    std::strcpy(reinterpret_cast<char*>(str),cache.portalName);
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
  auto  ground = dropRay(pos.x, pos.y+fallThreshold, pos.z, valid);
  auto  water  = waterRay(pos.x, pos.y, pos.z);
  float dY     = pY-ground;

  if(ground+waterDepthChest()<water) {
    setInAir(true);
    setAsSlide(false);
    return true;
    }
  if(dY>fallThreshold*2) {
    fallSpeed.x *=2;
    fallSpeed.z *=2;
    setInAir  (true);
    setAsSlide(false);
    return false;
    }

  const float lnorm = std::sqrt(norm.x*norm.x+norm.z*norm.z);;
  if(lnorm<0.01f || norm.y>=1.f || !testSlide(pos.x,pos.y+fallThreshold,pos.z)) {
    setAsSlide(false);
    return false;
    }

  const float timeK = float(dt)/100.f;
  const float speed = 0.02f*timeK*gravity;

  float k = std::fabs(norm.y)/lnorm;
  fallSpeed.x += +speed*norm.x*k;
  fallSpeed.y += -speed*std::sqrt(1.f - norm.y*norm.y);
  fallSpeed.z += +speed*norm.z*k;

  auto dp = fallSpeed*timeK;
  if(!tryMove(dp.x,dp.y,dp.z))
    tryMove(dp.x,0.f,dp.z);

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
  float timeK         = float(dt)/1000.f;
  float aceleration   = -gravity*timeK;
  if(0.f<fallCount) {
    fallSpeed/=(fallCount/1000.f);
    fallCount=-1.f;
    }
  fallSpeed.y+=aceleration;

  // check ground
  auto  pos      = npc.position();
  float pY       = pos.y;
  float chest    = canFlyOverWater() ? 0 : waterDepthChest();
  bool  valid    = false;
  auto  ground   = dropRay (pos.x, pos.y+fallThreshold, pos.z, valid);
  auto  water    = waterRay(pos.x, pos.y, pos.z);
  float fallStop = std::max(water-chest,ground);

  auto dp = fallSpeed*timeK;

  if(pY+dp.y>fallStop) {
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
    if(fallSpeed.y<-1500.f && !npc.isDead())
      npc.setAnim(AnimationSolver::FallDeep); else
    if(fallSpeed.y<-300.f && !npc.isDead())
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
  tickGravity(dt);
  if(fallSpeed.y<=0.f || !isInAir()) {
    setAsJumpup(false);
    //return;
    }

  float len=50;
  Tempest::Vec3 ret = {}, v={0,npc.translateY(),len};
  applyRotation(ret,v);

  if(testClimp(0.15f) &&
     testClimp(0.25f) &&
     testClimp(0.5f) &&
     testClimp(0.75f)){
    if(npc.setAnim(Npc::Anim::JumpHang)) {
      setAsJumpup(false);
      setAsClimb(true);
      return;
      }
    //tryMove(ret[0],ret[1],ret[2]);
    //return;
    }
  }

void MoveAlgo::tickClimb(uint64_t dt) {
  if(npc.bodyStateMasked()!=BS_CLIMB){
    setAsClimb(false);

    Tempest::Vec3 p={}, v={0,0,50};
    auto pos = npc.position();
    applyRotation(p,v);
    pos.x+=p.x;
    pos.z+=p.z;
    if(npc.testMove(pos,v,0))
      npc.setPosition(pos);
    clearSpeed();
    return;
    }
  Tempest::Vec3 v={};
  float k=1.5f;

  auto dp  = animMoveSpeed(dt);
  auto pos = npc.position();
  pos.x+=dp.x*k;
  //pos[1]+=dp[1];
  pos.z+=dp.z*k;

  if(npc.testMove(pos,v,0))
    npc.setPosition(pos);

  pos = npc.position();
  pos.y+=dp.y;
  if(npc.testMove(pos,v,0))
    npc.setPosition(pos);

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
    fallSpeed.x = gravity*dx/len;
    fallSpeed.y = 0.4f*gravity;
    fallSpeed.z = gravity*dz/len;
    fallCount   =-1.f;
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

Tempest::Vec3 MoveAlgo::npcMoveSpeed(uint64_t dt,MvFlags moveFlg) {
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
      return go2WpMoveSpeed(dp,npc.go2.wp->x,npc.go2.wp->z);
    }

  return dp;
  }

Tempest::Vec3 MoveAlgo::go2NpcMoveSpeed(const Tempest::Vec3& dp,const Npc& tg) {
  return go2WpMoveSpeed(dp,tg.position().x,tg.position().z);
  }

Tempest::Vec3 MoveAlgo::go2WpMoveSpeed(Tempest::Vec3 dp, float x, float z) {
  float dx   = x-npc.position().x;
  float dz   = z-npc.position().z;
  float qLen = (dx*dx+dz*dz);

  const float qSpeed = dp.x*dp.x+dp.z*dp.z;
  if(qSpeed>0.01f){
    float k = std::sqrt(std::min(qLen,qSpeed)/qSpeed);
    dp.x*=k;
    dp.z*=k;
    }
  return dp;
  }

bool MoveAlgo::testClimp(float scale) const {
  float len=100;
  Tempest::Vec3 ret = {}, orig = {0,0,len*scale}, v={};
  applyRotation(ret,orig);

  ret.x+=npc.x;
  ret.y+=npc.y + 150;//npc.translateY();
  ret.z+=npc.z;

  return npc.testMove(ret,v,0.f);
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
    char name[32]={};
    std::snprintf(name,sizeof(name),"SVM_%d_AARGH",int(npc.handle()->voice));
    npc.emitSoundEffect(name,25,true);
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

bool MoveAlgo::aiGoTo(const WayPoint *p,float destDist) {
  if(p==nullptr)
    return false;

  // use smaller threshold, to avoid edge-looping in script
  if(isClose(npc.position().x,npc.position().y,npc.position().z,*p,destDist))
    return false;

  npc.go2.set(p);
  return true;
  }

bool MoveAlgo::aiGoTo(Npc *p,float destDist) {
  if(p==nullptr)
    return false;

  float len = npc.qDistTo(*p);
  if(len<destDist*destDist)
    return false;

  npc.go2.set(p);
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

bool MoveAlgo::startClimb(JumpCode ani) {
  climbStart = npc.world().tickCount();
  climbPos0  = npc.position();
  jmp        = ani;

  if(jmp==JM_Up){
    setAsJumpup(true);
    setInAir(true);
    fallSpeed.x = 0.f;
    fallSpeed.y = 0.55f*gravity;
    fallSpeed.z = 0.f;
    fallCount   = 1000.f;
    }
  else if(jmp==JM_UpMid){
    setAsJumpup(false);
    setAsClimb(true);
    setInAir(true);
    }
  else if(jmp==JM_UpLow){
    setAsJumpup(false);
    setAsClimb(true);
    setInAir(true);
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

float MoveAlgo::dropRay(float x, float y, float z, bool &hasCol) const {
  if(std::fabs(cache.x-x)>eps || std::fabs(cache.y-y)>eps || std::fabs(cache.z-z)>eps) {
    auto ret         = npc.world().physic()->dropRay(x,y,z);
    cache.x          = x;
    cache.y          = y;
    cache.z          = z;
    cache.rayCastRet = ret.y();
    cache.mat        = ret.mat;
    cache.portalName = ret.sector!=nullptr ? ret.sector : cache.portalName;
    cache.hasCol     = ret.hasCol;
    if(ret.hasCol) {
      // store also normal
      cache.nx   = x;
      cache.ny   = y;
      cache.nz   = z;
      cache.norm = ret.n;
      }
    }
  hasCol = cache.hasCol;
  return cache.rayCastRet;
  }

float MoveAlgo::waterRay(float x, float y, float z) const {
  if(std::fabs(cache.wx-x)>eps || std::fabs(cache.wy-y)>eps || std::fabs(cache.wz-z)>eps) {
    auto ret     = npc.world().physic()->waterRay(x,y,z);
    cache.wx     = x;
    cache.wy     = y;
    cache.wz     = z;
    cache.wdepth = ret.y();
    }
  return cache.wdepth;
  }

Tempest::Vec3 MoveAlgo::normalRay(float x, float y, float z) const {
  if(std::fabs(cache.nx-x)>eps || std::fabs(cache.ny-y)>eps || std::fabs(cache.nz-z)>eps){
    cache.nx   = x;
    cache.ny   = y;
    cache.nz   = z;
    cache.norm = npc.world().physic()->landNormal(x,y,z);
    }

  return cache.norm;
  }

uint8_t MoveAlgo::groundMaterial(float x, float y, float z) const {
  if(std::fabs(cache.x-x)>eps || std::fabs(cache.y-y)>eps || std::fabs(cache.z-z)>eps) {
    auto r = npc.world().physic()->dropRay(x,y,z);
    return r.mat;
    }
  return cache.mat;
  }

uint8_t MoveAlgo::groundMaterial() const {
  const float stp = stepHeight();

  if(cache.wdepth+stp>cache.y)
    return ZenLoad::MaterialGroup::WATER;

  //make cache happy by addup fallThreshold
  const Tempest::Vec3 &p = npc.position();
  return groundMaterial(p.x,p.y+stp,p.z);
  }

Tempest::Vec3 MoveAlgo::groundNormal() const {
  const float stp = stepHeight();

  //make cache happy by addup fallThreshold
  const Tempest::Vec3 &p = npc.position();
  return normalRay(p.x,p.y+stp,p.z);
  }

const char* MoveAlgo::portalName() {
  return cache.portalName;
  }

