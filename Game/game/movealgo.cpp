#include "movealgo.h"

#include "world/world.h"
#include "world/npc.h"

const float MoveAlgo::slideBegin   =float(std::sin(40*M_PI/180));
const float MoveAlgo::fallThreshold=45.f;

MoveAlgo::MoveAlgo(Npc& unit, const World &w)
  :npc(unit),world(w) {
  }

void MoveAlgo::tick(uint64_t dt) {
  float dpos[3]={};
  auto  dp  = npc.animMoveSpeed(dt);
  auto  pos = npc.position();
  float rot = npc.rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);

  dpos[0]=dp.x;
  dpos[1]=npc.isFlyAnim() ? dp.y : 0;
  dpos[2]=dp.z;

  pos[0]+=mulSpeed*(dpos[0]*c-dpos[2]*s);
  pos[2]+=mulSpeed*(dpos[0]*s+dpos[2]*c);
  pos[1]+=dpos[1];

  aniSpeed[0]=dpos[0];
  aniSpeed[2]=dpos[2];
  float speed = std::sqrt(dpos[0]*dpos[0]+dpos[2]*dpos[2]);
  if(dpos[0]==0.f && dpos[1]==0.f && dpos[2]==0.f && isFrozen())
    return;
  setPos(pos,dt,speed);
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

void MoveAlgo::setSllideFaling(bool slide, bool faling) {
  if(isSlide()==slide && isFaling()==faling)
    return;

  if(faling)
    flags=Flags(flags|Faling); else
    flags=Flags(flags&(~Faling));
  if(slide)
    flags=Flags(flags|Slide);  else
    flags=Flags(flags&(~Slide));

  if(!isFaling() && !isSlide()){
    fallSpeed[0]=0;
    fallSpeed[1]=0;
    fallSpeed[2]=0;
    }
  }

void MoveAlgo::setPos(std::array<float,3> pos,uint64_t dt,float speed) {
  float               gravity=2*9.8f;
  std::array<float,3> fb=npc.position();
  std::array<float,3> norm={};
  bool                fallAni  = false;
  bool                slideAni = false;
  float               timeK    = float(dt)/1000.f;

  if(trySlide(fb,norm)){
    slideAni=true;
    pos = fb;
    fallSpeed[0]+=3.f*timeK*norm[0];
    fallSpeed[2]+=3.f*timeK*norm[2];
    switch(npc.tryMove(pos,fb,speed)){
      case Npc::MV_FAILED:  pos=npc.position(); break;
      case Npc::MV_CORRECT: pos=fb; break;
      case Npc::MV_OK:      break;
      }
    } else {
    slideAni=false;
    switch(npc.tryMove(pos,fb,speed)){
      case Npc::MV_FAILED:  pos=npc.position(); break;
      case Npc::MV_CORRECT: pos=fb; break;
      case Npc::MV_OK:      break;
      }
    }

  bool frozen=(npc.position()==pos && !isInAir());
  npc.setPosition(pos);

  bool valid  = false;
  auto oldY   = pos[1];
  auto ground = world.physic()->dropRay(pos[0],pos[1],pos[2],valid);
  setAsFrozen(frozen && isInAir());

  bool nFall=isFaling();
  setInAir(false);
  if(npc.isFlyAnim()) {
    if(oldY>ground){
      pos[1]=oldY;
      setInAir(true);
      } else {
      pos[1]=ground;
      nFall=false;
      }
    } else {
    float dY = (pos[1]-ground);
    if(dY<1) {
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

    if(dY>fallThreshold || npc.anim()==Npc::Fall)
      fallAni=true;
    if(dY>fallSpeed[1] && nFall)
      dY=fallSpeed[1];
    pos[0]+=fallSpeed[0];
    pos[1]-=dY;
    pos[2]+=fallSpeed[2];
    }

  if(!npc.isFlyAnim()) {
    switch(npc.tryMove(pos,fb,speed)) {
      case Npc::MV_CORRECT:
      case Npc::MV_OK: // fall anim
        npc.setPosition(fb);
        break;
      case Npc::MV_FAILED:
        fallAni=false;
        nFall=false;
        setInAir(false);
        break;
      }
    }

  setSllideFaling(slideAni,nFall);
  if(slideAni && !nFall) {
    npc.setAnim(Npc::Slide);
    }
  else if(nFall) {
    if(npc.anim()!=Npc::Jump)
      npc.setAnim(Npc::Fall);
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

  if(badFall || norm[1]<slideBegin) { // sliding
    pos[0]+=fallSpeed[0];
    pos[2]+=fallSpeed[2];
    return true;
    }
  return false;
  }

