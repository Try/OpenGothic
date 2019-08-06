#include "movealgo.h"

#include "world/world.h"
#include "world/npc.h"

const float MoveAlgo::closeToPointThreshold=50;
const float MoveAlgo::gravity              =100*9.8f;

MoveAlgo::MoveAlgo(Npc& unit)
  :npc(unit) {
  }

void MoveAlgo::tickMobsi(uint64_t dt) {
  if(npc.anim()!=AnimationSolver::Interact || npc.interactive()->isLoopState())
    return;

  auto dp  = animMoveSpeed(dt);
  auto pos = npc.position();
  pos[0]+=dp[0];
  pos[1]+=dp[1];
  pos[2]+=dp[2];
  npc.setPosition(pos);
  setAsSlide(false);
  setInAir  (false);
  }

bool MoveAlgo::tryMove(float x,float y,float z) {
  if(npc.isStanding() && !isSlide() && !isFaling()) {
    static const float eps = 1.f;
    if(std::fabs(x)<eps && std::fabs(y)<eps && std::fabs(z)<eps)
      return true;
    }
  return npc.tryMove({x,y,z});
  }

bool MoveAlgo::tickSlide(uint64_t dt) {
  float fallThreshold = stepHeight();

  if(isInAir())
    return false;

  auto  pos    = npc.position();
  auto  norm   = normalRay(pos[0],pos[1]+fallThreshold,pos[2]);
  // check ground
  float pY     = pos[1];
  bool  valid  = false;
  auto  ground = dropRay(pos[0], pos[1]+fallThreshold, pos[2], valid);
  float dY     = pY-ground;

  const float slideBegin = slideAngle();
  const float slideEnd   = slideAngle2();

  if(!(slideEnd<norm[1] && norm[1]<slideBegin)) { // sliding
    setAsSlide(false);
    return false;
    }

  if(dY>fallThreshold*2) {
    fallSpeed[0] *=2;
    fallSpeed[1] = 0.f;
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
  float chest    = waterDepthChest();
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
      npc.setAnim(AnimationSolver::Fall); else
    if(fallSpeed[1]<-100.f && npc.anim()==Npc::Anim::Jump)
      npc.setAnim(AnimationSolver::Fall);
    } else {
    if(ground+chest<water && !npc.isDead()) {
      // attach to water
      tryMove(0.f,water-pY,0.f);
      clearSpeed();
      setInAir(false);
      setInWater(true);
      setAsSwim(true);
      } else {
      // attach to ground
      tryMove(0.f,ground-pY,0.f);
      clearSpeed();
      setInAir(false);
      }
    }
  }

void MoveAlgo::tickClimb(uint64_t dt) {
  tickGravity(dt);
  if(fallSpeed[1]<=0.f || !isInAir())
    setAsClimb(false);
  }

void MoveAlgo::tickSwim(uint64_t dt) {
  auto  dp            = npcMoveSpeed(dt);
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
    npc.setAnim(npc.anim());
    return;
    }

  if(npc.isDead()){
    setAsSwim(false);
    return;
    }

  // swim on top of water
  tryMove(dp[0],water-pY,dp[2]);
  }

void MoveAlgo::tick(uint64_t dt) {
  if(npc.interactive()!=nullptr)
    return tickMobsi(dt);

  if(isClimb())
    return tickClimb(dt);

  if(isSwim())
    return tickSwim(dt);

  if(isInAir() && !npc.isFlyAnim()) {
    return tickGravity(dt);
    }

  if(isSlide()){
    if(tickSlide(dt))
      return;
    if(isInAir())
      return;
    }

  auto  dp            = npcMoveSpeed(dt);
  auto  pos           = npc.position();
  float pY            = pos[1];
  float fallThreshold = stepHeight();

  // moving NPC, by animation
  bool  valid  = false;
  auto  ground = dropRay (pos[0]+dp[0], pos[1]+dp[1]+fallThreshold, pos[2]+dp[2], valid);
  auto  water  = waterRay(pos[0]+dp[0], pos[1]+dp[1], pos[2]+dp[2]);
  float dY     = pY-ground;

  if(!npc.isDead() && ground+waterDepthChest()<water){
    setInWater(true);
    setAsSwim(true);
    npc.setAnim(npc.anim());
    return;
    }
  if(ground+waterDepthKnee()<water)
    setInWater(true); else
    setInWater(false);

  if(0.f<dY && npc.isFlyAnim()) {
    // jump animation
    tryMove(dp[0],dp[1],dp[2]);

    fallSpeed[0] += dp[0];
    fallSpeed[1] += dp[1];
    fallSpeed[2] += dp[2];
    fallCount    += dt;
    setInAir  (true);
    setAsSlide(false);
    }
  else if(0.f<=dY && dY<fallThreshold) {
    if(tickSlide(dt))
      return;
    // move down the ramp
    if(!tryMove(dp[0],-dY,dp[2])){
      if(!tryMove(dp[0],dp[1],dp[2]))
        onMoveFailed();
      }
    setInAir  (false);
    setAsSlide(false);
    }
  else if(-fallThreshold<dY && dY<0.f) {
    if(tickSlide(dt))
      return;
    // move up the ramp
    if(!tryMove(dp[0],-dY,dp[2]))
      onMoveFailed();
    setInAir  (false);
    setAsSlide(false);
    }
  else if(0.f<dY) {
    const bool walk = bool(npc.walkMode()&WalkBit::WM_Walk);
    if((!walk && npc.isPlayer()) ||
       (dY<300.f && !npc.isPlayer())) {
      // start to fall of cliff
      tryMove(dp[0],0,dp[2]);
      fallSpeed[0] = 0.3f*dp[0];
      fallSpeed[1] = 0.f;
      fallSpeed[2] = 0.3f*dp[2];
      fallCount    = dt;
      setInAir  (true);
      setAsSlide(false);
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

std::array<float,3> MoveAlgo::npcMoveSpeed(uint64_t dt) {
  std::array<float,3> dp = animMoveSpeed(dt);
  if(!npc.isFlyAnim())
    dp[1] = 0.f;

  const float qSpeed = dp[0]*dp[0]+dp[2]*dp[2];

  if(currentGoTo) {
    float dx   = currentGoTo->x-npc.position()[0];
    float dz   = currentGoTo->z-npc.position()[2];
    float qLen = (dx*dx+dz*dz);

    if(qLen<=qSpeed || qLen<closeToPointThreshold*closeToPointThreshold){
      currentGoTo=nullptr;
      npc.setAnim(AnimationSolver::Idle);
      } else {
      npc.setAnim(AnimationSolver::Move);
      }
    if(qSpeed>0.01f){
      float k = std::sqrt(std::min(qLen,qSpeed)/qSpeed);
      dp[0]*=k;
      dp[2]*=k;
      }
    }
  else if(currentGoToNpc) {
    float dx   = currentGoToNpc->position()[0]-npc.position()[0];
    float dz   = currentGoToNpc->position()[2]-npc.position()[2];
    float qLen = (dx*dx+dz*dz);

    if(npc.checkGoToNpcdistance(*currentGoToNpc)){
      currentGoToNpc=nullptr;
      npc.setAnim(AnimationSolver::Idle);
      } else {
      npc.setAnim(AnimationSolver::Move);
      }
    if(qSpeed>0.01f){
      float k = std::sqrt(std::min(qLen,qSpeed)/qSpeed);
      dp[0]*=k;
      dp[2]*=k;
      }
    }

  return dp;
  }

float MoveAlgo::stepHeight() const {
  auto gl = npc.guild();
  auto v  = npc.world().script().guildVal().step_height[gl];
  if(v>0.f)
    return v;
  return 50;
  }

float MoveAlgo::slideAngle() const {
  auto  gl = npc.guild();
  float k  = float(M_PI)/180.f;
  return std::sin((90-npc.world().script().guildVal().slide_angle[gl])*k);
  }

float MoveAlgo::slideAngle2() const {
  auto  gl = npc.guild();
  float k  = float(M_PI)/180.f;
  return std::sin(npc.world().script().guildVal().slide_angle2[gl]*k);
  }

float MoveAlgo::waterDepthKnee() const {
  auto gl = npc.guild();
  return npc.world().script().guildVal().water_depth_knee[gl];
  }

float MoveAlgo::waterDepthChest() const {
  auto gl = npc.guild();
  return npc.world().script().guildVal().water_depth_chest[gl];
  }

bool MoveAlgo::isClose(const std::array<float,3> &w, const WayPoint &p) {
  return isClose(w[0],w[1],w[2],p);
  }

bool MoveAlgo::isClose(float x, float /*y*/, float z, const WayPoint &p) {
  float len = p.qDistTo(x,p.y,z);
  return (len<closeToPointThreshold*closeToPointThreshold);
  }

bool MoveAlgo::aiGoTo(const WayPoint *p) {
  currentGoTo    = p;
  currentGoToNpc = nullptr;
  if(p==nullptr)
    return false;

  if(isClose(npc.position()[0],npc.position()[1],npc.position()[2],*p)){
    currentGoTo=nullptr;
    return false;
    }
  return true;
  }

bool MoveAlgo::aiGoTo(const Npc *p,float destDist) {
  currentGoToNpc = p;
  currentGoTo    = nullptr;
  if(p==nullptr)
    return false;
  float len = npc.qDistTo(*p);
  if(len<destDist*destDist){
    currentGoToNpc=nullptr;
    return false;
    }
  return true;
  }

void MoveAlgo::aiGoTo(const std::nullptr_t) {
  currentGoTo   =nullptr;
  currentGoToNpc=nullptr;
  }

bool MoveAlgo::startClimb(JumpCode ani) {
  climbStart = npc.world().tickCount();
  climbPos0  = npc.position();
  jmp        = ani;

  if(jmp==JM_Up){
    setAsClimb(true);
    setInAir(true);
    fallSpeed[0]=0.f;
    fallSpeed[1]=0.75f*gravity;
    fallSpeed[2]=0.f;
    fallCount   =-1.f;
    }
  else if(jmp==JM_UpMid){
    setAsClimb(true);
    setInAir(true);
    fallSpeed[0]=0.f;
    fallSpeed[1]=0.4f*gravity;
    fallSpeed[2]=0.f;
    fallCount   =-1.f;
    }
  return true;
  }

bool MoveAlgo::hasGoTo() const {
  return currentGoTo!=nullptr || currentGoToNpc!=nullptr;
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

bool MoveAlgo::isClimb() const {
  return flags&Climb;
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

void MoveAlgo::setAsClimb(bool f) {
  if(f)
    flags=Flags(flags|Climb); else
    flags=Flags(flags&(~Climb));
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
    flags=Flags(flags|Swim);  else
    flags=Flags(flags&(~Swim));
  }

bool MoveAlgo::processClimb() {
  if(!isClimb())
    return false;
  float dspeed = 35.f;

  if(climbHeight<=0.f)
    climbHeight=npc.clampHeight(npc.anim());

  if(npc.anim()==AnimationSolver::Idle && !npc.isFlyAnim()) {
    auto  pos = climbPos0;
    float rot = npc.rotationRad();
    float s   = std::sin(rot), c = std::cos(rot);
    pos[0]+= dspeed*s;
    pos[2]+=-dspeed*c;

    auto ground = npc.world().physic()->dropRay(pos[0],pos[1]+climbHeight+10,pos[2]);
    if(ground.hasCol) {
      pos[1]=ground.y();
      std::array<float,3> fb={};
      if(npc.testMove(pos,fb,0))
        npc.setPosition(fb);
      }
    setAsClimb(false);
    climbHeight=0;
    return true;
    }

  auto pos = npc.position();
  npc.setPosition(pos);
  return true;
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
  static const float eps = 0.001f;

  if(std::fabs(cache.x-x)>eps || std::fabs(cache.y-y)>eps || std::fabs(cache.z-z)>eps) {
    auto ret         = npc.world().physic()->dropRay(x,y,z);
    cache.x          = x;
    cache.y          = y;
    cache.z          = z;
    cache.hasCol     = ret.hasCol;
    cache.rayCastRet = ret.y();
    }
  hasCol = cache.hasCol;
  return cache.rayCastRet;
  }

float MoveAlgo::waterRay(float x, float y, float z) const {
  static const float eps = 0.001f;

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
  static const float eps = 0.1f;

  if(std::fabs(cache.nx-x)>eps || std::fabs(cache.ny-y)>eps || std::fabs(cache.nz-z)>eps){
    cache.nx   = x;
    cache.ny   = y;
    cache.nz   = z;
    cache.norm = npc.world().physic()->landNormal(x,y,z);
    }

  return cache.norm;
  }

uint8_t MoveAlgo::groundMaterial() const {
  const std::array<float,3> &pos = npc.position();
  auto r = npc.world().physic()->dropRay(pos[0],pos[1],pos[2]);
  return r.mat;
  }

