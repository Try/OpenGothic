#include "movealgo.h"
#include "serialize.h"

#include "world/world.h"
#include "world/npc.h"

const float   MoveAlgo::closeToPointThreshold = 50;
const float   MoveAlgo::gravity               = DynamicWorld::gravity;
const float   MoveAlgo::eps                   = 1.f; // 1-milimeter
const int32_t MoveAlgo::flyOverWaterHint      = 999999;

MoveAlgo::MoveAlgo(Npc& unit)
  :npc(unit) {
  }

void MoveAlgo::load(Serialize &fin) {
  fin.read(reinterpret_cast<uint32_t&>(flags));
  fin.read(mulSpeed,fallSpeed,fallCount,climbStart,climbPos0,climbHeight);
  fin.read(reinterpret_cast<uint8_t&>(jmp));
  }

void MoveAlgo::save(Serialize &fout) const {
  fout.write(uint32_t(flags));
  fout.write(mulSpeed,fallSpeed,fallCount,climbStart,climbPos0,climbHeight);
  fout.write(uint8_t(jmp));
  }

void MoveAlgo::tickMobsi(uint64_t dt) {
  if(npc.interactive()->isStaticState())
    return;

  auto dp  = animMoveSpeed(dt);
  auto pos = npc.position();
  pos[0]+=dp[0];
  //pos[1]+=dp[1];
  pos[2]+=dp[2];
  npc.setPosition(pos);
  setAsSlide(false);
  setInAir  (false);
  }

bool MoveAlgo::tryMove(float x,float y,float z) {
  if(npc.isStanding() && !isSlide() && !isFaling()) {
    if(std::fabs(x)<eps && std::fabs(y)<eps && std::fabs(z)<eps)
      return true;
    }
  return npc.tryMove({x,y,z});
  }

bool MoveAlgo::tickSlide(uint64_t dt) {
  float fallThreshold = stepHeight();
  auto  pos           = npc.position();

  auto  norm   = normalRay(pos[0],pos[1]+fallThreshold,pos[2]);
  // check ground
  float pY     = pos[1];
  bool  valid  = false;
  auto  ground = dropRay(pos[0], pos[1]+fallThreshold, pos[2], valid);
  float dY     = pY-ground;

  //if(ground+chest<water)
  //  ;
  if(dY>fallThreshold*2) {
    fallSpeed[0] *=2;
    fallSpeed[2] *=2;
    setInAir  (true);
    setAsSlide(false);
    return false;
    }

  if(!isSlide()) {
    fallSpeed[0] = 0.f;
    fallSpeed[1] = 0.f;
    fallSpeed[2] = 0.f;
    fallCount=-1.f;
    }

  if(!testSlide(pos[0],pos[1]+fallThreshold,pos[2])) {
    setAsSlide(false);
    return false;
    }

  const float timeK = float(dt)/100.f;
  const float speed = 0.02f*timeK*gravity;

  float k = std::fabs(norm[1])/std::sqrt(norm[0]*norm[0]+norm[2]*norm[2]);
  fallSpeed[0] += +speed*norm[0]*k;
  fallSpeed[1] += -speed*std::sqrt(1.f - norm[1]*norm[1]);
  fallSpeed[2] += +speed*norm[2]*k;

  auto dp = fallSpeed;
  dp[0]*=timeK;
  dp[1]*=timeK;
  dp[2]*=timeK;

  if(!tryMove(dp[0],dp[1],dp[2]))
    tryMove(dp[0],0.f,dp[2]);

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
    fallSpeed[0]/=(fallCount/1000.f);
    fallSpeed[1]/=(fallCount/1000.f);
    fallSpeed[2]/=(fallCount/1000.f);
    fallCount=-1.f;
    }
  fallSpeed[1]+=aceleration;

  // check ground
  auto  pos      = npc.position();
  float pY       = pos[1];
  float chest    = canFlyOverWater() ? 0 : waterDepthChest();
  bool  valid    = false;
  auto  ground   = dropRay(pos[0], pos[1]+fallThreshold, pos[2], valid);
  auto  water    = waterRay(pos[0], pos[1], pos[2]);
  float fallStop = std::max(water-chest,ground);

  auto dp = fallSpeed;
  dp[0]*=timeK;
  dp[1]*=timeK;
  dp[2]*=timeK;

  if(pY+dp[1]>fallStop) {
    // continue falling
    if(!tryMove(dp[0],dp[1],dp[2])) {
      fallSpeed[1]=0.f;
      if((std::fabs(dp[0])<0.001f && std::fabs(dp[2])<0.001f) || !tryMove(dp[0],0.f,dp[2])) {
        // attach to ground
        setInAir(false);
        npc.setAnim(AnimationSolver::Idle);
        return;
        }
      setInAir(false);
      }
    if(fallSpeed[1]<-1500.f)
      npc.setAnim(AnimationSolver::FallDeep); else
    if(fallSpeed[1]<-300.f)
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
  if(fallSpeed[1]<=0.f || !isInAir()) {
    setAsJumpup(false);
    return;
    }

  float len=50;
  std::array<float,3> ret = {}, v={0,npc.translateY(),len};
  applyRotation(ret,v.data());

  if(testClimp(0.25f) &&
     testClimp(0.50f) &&
     testClimp(0.75f) &&
     testClimp(1.f)){
    setAsJumpup(false);
    tryMove(ret[0],ret[1],ret[2]);
    return;
    }
  }

void MoveAlgo::tickSwim(uint64_t dt) {
  auto  dp            = npcMoveSpeed(dt,MvFlags::NoFlag);
  auto  pos           = npc.position();
  float pY            = pos[1];
  float fallThreshold = stepHeight();
  auto  chest         = waterDepthChest();

  bool  valid  = false;
  auto  ground = dropRay (pos[0]+dp[0], pos[1]+dp[1]+fallThreshold, pos[2]+dp[2], valid);
  auto  water  = waterRay(pos[0]+dp[0], pos[1]+dp[1]-chest, pos[2]+dp[2]);

  if(ground+chest>=water){
    setAsSwim(false);
    tryMove(dp[0],ground-pY,dp[2]);
    //npc.setAnim(npc.anim()); // TODO: reset anim
    return;
    }

  if(npc.isDead()){
    setAsSwim(false);
    return;
    }

  // swim on top of water
  tryMove(dp[0],water-pY,dp[2]);
  }

void MoveAlgo::tick(uint64_t dt, MvFlags moveFlg) {
  if(npc.interactive()!=nullptr)
    return tickMobsi(dt);

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

  auto  dp            = npcMoveSpeed(dt,moveFlg);
  auto  pos           = npc.position();
  float pY            = pos[1];
  float fallThreshold = stepHeight();

  // moving NPC, by animation
  bool  valid   = false;
  auto  ground  = dropRay (pos[0]+dp[0], pos[1]+dp[1]+fallThreshold, pos[2]+dp[2], valid);
  auto  water   = waterRay(pos[0]+dp[0], pos[1]+dp[1], pos[2]+dp[2]);
  float dY      = pY-ground;
  bool  onGound = true;

  if(!npc.isDead() && ground+waterDepthChest()<water){
    setInWater(true);
    setAsSwim(true);
    //npc.setAnim(npc.anim()); //TODO: reset anim
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
    tryMove(dp[0],dp[1],dp[2]);
    fallSpeed[0] += dp[0];
    fallSpeed[1] += dp[1];
    fallSpeed[2] += dp[2];
    fallCount    += float(dt);
    setInAir  (true);
    setAsSlide(false);
    }
  else if(0.f<=dY && dY<fallThreshold) {
    if(onGound && testSlide(pos[0]+dp[0], pos[1]+dp[1]+fallThreshold, pos[2]+dp[2])) {
      tryMove(dp[0],-dY,dp[2]);
      setAsSlide(true);
      return;
      }
    // move down the ramp
    if(!tryMove(dp[0],-dY,dp[2])){
      if(!tryMove(dp[0],dp[1],dp[2]))
        onMoveFailed();
      }
    setInAir  (false);
    setAsSlide(false);
    }
  else if(-fallThreshold<dY && dY<0.f) {
    if(onGound && testSlide(pos[0]+dp[0], pos[1]+dp[1]+fallThreshold, pos[2]+dp[2])) {
      return;
      }
    // move up the ramp
    if(!tryMove(dp[0],-dY,dp[2]))
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
      if(tryMove(dp[0],dp[1],dp[2])){
        fallSpeed[0] = 0.3f*dp[0];
        fallSpeed[1] = 0.f;
        fallSpeed[2] = 0.3f*dp[2];
        fallCount    = float(dt);
        setInAir  (true);
        setAsSlide(false);
        } else {
        tryMove(dp[0],0,dp[2]);
        }
      }
    }
  }

void MoveAlgo::clearSpeed() {
  fallSpeed[0]=0;
  fallSpeed[1]=0;
  fallSpeed[2]=0;
  fallCount   =-1.f;
  }

void MoveAlgo::accessDamFly(float dx, float dz) {
  if(flags==0) {
    float len = std::sqrt(dx*dx+dz*dz);
    fallSpeed[0]=gravity*dx/len;
    fallSpeed[1]=0.4f*gravity;
    fallSpeed[2]=gravity*dz/len;
    fallCount   =-1.f;
    setInAir(true);
    }
  }

void MoveAlgo::applyRotation(std::array<float,3> &out, float* dpos) const {
  float rot = npc.rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);
  out[0] = -mulSpeed*(dpos[0]*c-dpos[2]*s);
  out[2] = -mulSpeed*(dpos[0]*s+dpos[2]*c);
  out[1] = dpos[1];
  }

std::array<float,3> MoveAlgo::animMoveSpeed(uint64_t dt) const {
  auto dp = npc.animMoveSpeed(dt);
  std::array<float,3> ret;
  applyRotation(ret,dp.v);
  return ret;
  }

std::array<float,3> MoveAlgo::npcMoveSpeed(uint64_t dt,MvFlags moveFlg) {
  std::array<float,3> dp = animMoveSpeed(dt);
  if(!npc.isJumpAnim())
    dp[1] = 0.f;

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

std::array<float,3> MoveAlgo::go2NpcMoveSpeed(const std::array<float,3>& dp,const Npc& tg) {
  return go2WpMoveSpeed(dp,tg.position()[0],tg.position()[2]);
  }

std::array<float,3> MoveAlgo::go2WpMoveSpeed(std::array<float,3> dp, float x, float z) {
  float dx   = x-npc.position()[0];
  float dz   = z-npc.position()[2];
  float qLen = (dx*dx+dz*dz);

  const float qSpeed = dp[0]*dp[0]+dp[2]*dp[2];
  if(qSpeed>0.01f){
    float k = std::sqrt(std::min(qLen,qSpeed)/qSpeed);
    dp[0]*=k;
    dp[2]*=k;
    }
  return dp;
  }

bool MoveAlgo::testClimp(float scale) const {
  float len=100;
  std::array<float,3> ret = {}, v={0,0,len*scale};
  applyRotation(ret,v.data());

  ret[0]+=npc.x;
  ret[1]+=npc.y+npc.translateY();
  ret[2]+=npc.z;

  return npc.testMove(ret,v,0.f);
  }

bool MoveAlgo::testSlide(float x,float y,float z) const {
  if(isInAir() || npc.isJumpAnim())
    return false;

  auto  norm             = normalRay(x,y,z);
  // check ground
  const float slideBegin = slideAngle();
  const float slideEnd   = slideAngle2();

  if(!(slideEnd<norm[1] && norm[1]<slideBegin)) {
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

  float speed       = fallSpeed[1];
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

bool MoveAlgo::isClose(const std::array<float,3> &w, const WayPoint &p) {
  return isClose(w[0],w[1],w[2],p);
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
  if(isClose(npc.position()[0],npc.position()[1],npc.position()[2],*p,destDist))
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
  //npc.currentGoToNpc = nullptr;
  //npc.currentGoTo    = nullptr;

  auto p = npc.currentTarget;
  if(p==nullptr)
    return false;
  float len = npc.qDistTo(*p);
  if(len<destDist*destDist){
    return false;
    }
  //npc.setAnim(Npc::Anim::Move);
  return true;
  }

bool MoveAlgo::startClimb(JumpCode ani) {
  climbStart = npc.world().tickCount();
  climbPos0  = npc.position();
  jmp        = ani;

  if(jmp==JM_Up){
    setAsJumpup(true);
    setInAir(true);
    fallSpeed[0]=0.f;
    fallSpeed[1]=0.55f*gravity;
    fallSpeed[2]=0.f;
    fallCount   =1000.f;
    }
  else if(jmp==JM_UpMid){
    setAsJumpup(false);
    setInAir(true);
    }
  else if(jmp==JM_UpLow){
    setAsJumpup(false);
    setInAir(true);
    }
  return true;
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

bool MoveAlgo::isInWater() const {
  return flags&InWater;
  }

bool MoveAlgo::isSwim() const {
  return flags&Swim;
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
  if(f)
    npc.closeWeapon(true);
  if(f)
    flags=Flags(flags|Swim);  else
    flags=Flags(flags&(~Swim));
  }

bool MoveAlgo::slideDir() const {
  float a = std::atan2(fallSpeed[0],fallSpeed[2])+float(M_PI/2);
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
  static const float eps = 0.1f;

  if(std::fabs(cache.wx-x)>eps || std::fabs(cache.wy-y)>eps || std::fabs(cache.wz-z)>eps) {
    auto ret     = npc.world().physic()->waterRay(x,y,z);
    cache.wx     = x;
    cache.wy     = y;
    cache.wz     = z;
    cache.wdepth = ret.y();
    }
  return cache.wdepth;
  }

std::array<float,3> MoveAlgo::normalRay(float x, float y, float z) const {
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
  const std::array<float,3> &p = npc.position();
  return groundMaterial(p[0],p[1]+stp,p[2]);
  }

std::array<float,3> MoveAlgo::groundNormal() const {
  const float stp = stepHeight();

  //make cache happy by addup fallThreshold
  const std::array<float,3> &p = npc.position();
  return normalRay(p[0],p[1]+stp,p[2]);
  }

