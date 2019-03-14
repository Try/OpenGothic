#include "movealgo.h"

#include "world/world.h"
#include "world/npc.h"

const float MoveAlgo::slideBegin   =float(std::sin(40*M_PI/180));
const float MoveAlgo::slideEnd     =float(std::sin(0*M_PI/180));
const float MoveAlgo::slideSpeed   =11.f;
const float MoveAlgo::fallThreshold=45.f;

MoveAlgo::MoveAlgo(Npc& unit, const World &w)
  :npc(unit),world(w) {
  }

void MoveAlgo::tick(uint64_t dt) {
  float dpos[3]={};
  auto  dp  = npc.animMoveSpeed(dt);

  if(hasGoTo() && npc.isStanding()){
    dp = npc.animMoveSpeed(dt);
    }

  if(!isClimb()) {
    dpos[0]=dp.x;
    dpos[1]=npc.isFlyAnim() ? dp.y : 0;
    dpos[2]=dp.z;
    }

  if((npc.isStanding() || (dpos[0]==0.f && dpos[1]==0.f && dpos[2]==0.f)) && !hasGoTo() && isFrozen())
    return;

  float speed = std::sqrt(dpos[0]*dpos[0]+dpos[2]*dpos[2]);
  if(currentGoTo) {
    float dx  = currentGoTo->position.x-npc.position()[0];
    //float dy  = currentGoTo->position.y-npc.position()[1];
    float dz  = currentGoTo->position.z-npc.position()[2];
    float len = std::sqrt(dx*dx+dz*dz);

    if(len<=speed){
      currentGoTo=nullptr;
      npc.setAnim(AnimationSolver::Idle);
      } else {
      npc.setAnim(AnimationSolver::Move);
      }
    if(len>0.1f){
      float k = std::min(len,mulSpeed*speed)/len;
      aniSpeed[0] = dx*k;
      aniSpeed[2] = dz*k;
      }
    }
  else if(currentGoToNpc) {
    float dx  = currentGoToNpc->position()[0]-npc.position()[0];
    float dy  = currentGoToNpc->position()[1]-npc.position()[1];
    float dz  = currentGoToNpc->position()[2]-npc.position()[2];
    float len = (dx*dx+dy*dy+dz*dz);

    if(npc.checkGoToNpcdistance(*currentGoToNpc)){
      currentGoToNpc=nullptr;
      npc.setAnim(AnimationSolver::Idle);
      } else {
      npc.setAnim(AnimationSolver::Move);
      }
    if(len>0.1f*0.1f){
      len = std::sqrt(len);
      float k = std::min(len,mulSpeed*speed)/len;
      aniSpeed[0] = dx*k;
      aniSpeed[2] = dz*k;
      }
    }
  else {
    float rot = npc.rotationRad();
    float s   = std::sin(rot), c = std::cos(rot);
    aniSpeed[0]=-mulSpeed*(dpos[0]*c-dpos[2]*s);
    aniSpeed[2]=-mulSpeed*(dpos[0]*s+dpos[2]*c);
    aniSpeed[1]=dpos[1];
    }

  auto pos = npc.position();
  pos[0]+=aniSpeed[0];
  pos[2]+=aniSpeed[2];
  pos[1]+=dpos[1];

  setPos(pos,dt,speed);
  }

void MoveAlgo::clearSpeed() {
  fallSpeed[0]=0;
  fallSpeed[1]=0;
  fallSpeed[2]=0;
  flags = NoFlags;
  }

bool MoveAlgo::aiGoTo(const ZenLoad::zCWaypointData *p) {
  currentGoTo    = p;
  currentGoToNpc = nullptr;
  if(p==nullptr)
    return false;
  float len = npc.qDistTo(currentGoTo->position.x,npc.position()[1],currentGoTo->position.z);
  if(len<10*10){
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

bool MoveAlgo::startClimb() {
  climbStart=world.tickCount();
  climbPos0 =npc.position();
  setAsClimb(true);
  setAsFrozen(false);
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

bool MoveAlgo::isFrozen() const {
  return flags&Frozen;
  }

bool MoveAlgo::isInAir() const {
  return flags&InAir;
  }

bool MoveAlgo::isClimb() const {
  return flags&Climb;
  }

void MoveAlgo::setAsFrozen(bool f) {
  if(f)
    flags=Flags(flags|Frozen); else
    flags=Flags(flags&(~Frozen));
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

void MoveAlgo::setSllideFaling(bool slide, bool faling) {
  if(isSlide()==slide && isFaling()==faling)
    return;

  if(faling)
    flags=Flags(flags|Faling); else
    flags=Flags(flags&(~Faling));
  if(slide)
    flags=Flags(flags|Slide);  else
    flags=Flags(flags&(~Slide));
  }

bool MoveAlgo::processClimb() {
  if(!isClimb())
    return false;
  float dspeed = 35.f;
  bool  valid  = false;

  if(climbHeight<=0.f)
    climbHeight=npc.clampHeight(npc.anim());

  if(npc.anim()==AnimationSolver::Idle && !npc.isFlyAnim()) {
    auto  pos = climbPos0;
    float rot = npc.rotationRad();
    float s   = std::sin(rot), c = std::cos(rot);
    pos[0]+= dspeed*s;
    pos[2]+=-dspeed*c;

    auto ground = world.physic()->dropRay(pos[0],pos[1]+climbHeight+10,pos[2],valid);
    if(valid) {
      pos[1]=ground;
      std::array<float,3> fb={};
      if(npc.tryMove(pos,fb,0))
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

void MoveAlgo::setPos(std::array<float,3> pos,uint64_t dt,float speed) {
  float               gravity=100*9.8f;
  std::array<float,3> fb=npc.position();
  std::array<float,3> norm={};
  bool                fallAni  = false;
  bool                slideAni = false;
  float               timeK    = float(dt)/1000.f;

  if(processClimb())
    return;

  auto oldY = npc.position()[1];
  if(trySlide(fb,norm)){
    slideAni=true;
    fallSpeed[0]+=slideSpeed*norm[0];
    fallSpeed[2]+=slideSpeed*norm[2];
    } else {
    slideAni=false;
    switch(npc.tryMove(pos,fb,speed*0.5f)){
      case Npc::MV_FAILED:  {
        pos[0]=npc.position()[0];
        pos[2]=npc.position()[2];
        switch(npc.tryMove(pos,fb,speed*0.5f)){
          case Npc::MV_FAILED:  pos=npc.position(); break;
          case Npc::MV_CORRECT: pos=fb; break;
          case Npc::MV_OK:      break;
          }
        break;
        }
      case Npc::MV_CORRECT: pos=fb; break;
      case Npc::MV_OK:      break;
      }
    npc.setPosition(pos);
    }

  bool frozen=(npc.position()==pos && !isInAir());
  oldY = std::max(oldY,pos[1]);

  bool valid  = false;
  auto ground = world.physic()->dropRay(pos[0],oldY,pos[2],valid);
  setAsFrozen(frozen && !isInAir());

  bool nFall=isFaling();
  setInAir(false);
  if(npc.isFlyAnim()) {
    if(oldY>ground) {
      fallSpeed[1] = aniSpeed[1];
      setInAir(true);
      } else {
      pos[1]=ground;
      nFall=false;
      }
    setAsFrozen(false);
    } else {
    float fallY = fallSpeed[1]*timeK;
    float dY    = (pos[1]-ground);
    if(dY<fallY) {
      fallSpeed[1]=0;
      nFall=false;
      } else {
      float aceleration=gravity*timeK;
      if(nFall) {
        fallSpeed[1]+=aceleration;
        setInAir(true);
        }
      if(dY>fallThreshold)
        nFall=true;
      }

    if(dY>fallThreshold || npc.anim()==AnimationSolver::Fall)
      fallAni=true;
    if(dY>fallY && nFall)
      dY=fallY;
    pos[0]+=fallSpeed[0]*timeK;
    pos[1]-=dY;
    pos[2]+=fallSpeed[2]*timeK;
    }

  switch(npc.tryMoveVr(pos,fb,5.f)) {
    case Npc::MV_CORRECT:
    case Npc::MV_OK: // fall anim
      npc.setPosition(fb);
      break;
    case Npc::MV_FAILED:
      fallAni=false;
      slideAni=false;
      nFall=false;
      fallSpeed[0]=0;
      fallSpeed[1]=0;
      fallSpeed[2]=0;
      setInAir(false);
      break;
    }

  setSllideFaling(slideAni,nFall);
  if(slideAni && !nFall) {
    if(slideDir())
      npc.setAnim(AnimationSolver::SlideA); else
      npc.setAnim(AnimationSolver::SlideB);
    }
  else if(nFall) {
    if(fallSpeed[0]!=0.f || fallSpeed[1]!=0.f || fallSpeed[2]!=0.f){
      if(fallSpeed[1]>1500.f) {
        npc.setAnim(AnimationSolver::FallDeep);
        }
      else if((npc.anim()!=AnimationSolver::Jump || fallSpeed[1]>300.f) && npc.anim()!=AnimationSolver::Fall) {
        if(npc.anim()==AnimationSolver::Jump) {
          fallSpeed[0] += aniSpeed[0]/timeK;
          fallSpeed[1] += aniSpeed[1]/timeK;
          fallSpeed[2] += aniSpeed[2]/timeK;
          }
        npc.setAnim(AnimationSolver::Fall);
        }
      }
    }
  else if(!nFall && !slideAni && !npc.isFlyAnim()) {
    fallSpeed[0]=0;
    fallSpeed[1]=0;
    fallSpeed[2]=0;
    }

  if(slideAni || nFall)
    setAsFrozen(false);
  }

bool MoveAlgo::trySlide(std::array<float,3>& pos,std::array<float,3>& norm) {
  if(npc.isFlyAnim())
    return false;

  bool  badFall = false;
  bool  valid   = false;
  auto  ground  = world.physic()->dropRay(pos[0],pos[1],pos[2],valid);
  float delta   = (pos[1]-ground);

  if(valid && delta>fallThreshold) {
    std::array<float,3> drop=pos;
    drop[1]-=std::min(fallSpeed[1],delta);
    if(npc.tryMove(drop,drop,0.f))
      return false;

    badFall=true;
    }

  norm = world.physic()->landNormal(pos[0],pos[1],pos[2]);

  if(badFall || (slideEnd<norm[1] && norm[1]<slideBegin)) { // sliding
    return true;
    }
  return false;
  }

bool MoveAlgo::slideDir() const {
  float a = std::atan2(fallSpeed[0],fallSpeed[2])+float(M_PI/2);
  float b = npc.rotationRad();

  auto s = std::sin(a-b);
  return s>0;
  }
