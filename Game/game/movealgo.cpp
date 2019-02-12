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

  pSpeed = std::sqrt(dpos[0]*dpos[0]+dpos[2]*dpos[2]);
  if(dpos[0]==0.f && dpos[1]==0.f && dpos[2]==0.f && isFrozen())
    return;
  setPos(pos,dt,pSpeed);
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

void MoveAlgo::setAsFrozen(bool f) {
  if(f)
    flags=Flags(flags|Frozen); else
    flags=Flags(flags&(~Frozen));
  }

void MoveAlgo::setAsFaling(bool f) {
  if(isFaling()==f)
    return;
  fallSpeed[1]=0;
  if(f)
    flags=Flags(flags|Faling); else
    flags=Flags(flags&(~Faling));

  if(!isFaling() && !isSlide()){
    fallSpeed[0]=0;
    fallSpeed[2]=0;
    }
  }

void MoveAlgo::setAsSlide(bool f) {
  if(isSlide()==f)
    return;

  if(f)
    flags=Flags(flags|Slide);  else
    flags=Flags(flags&(~Slide));

  if(!isFaling() && !isSlide()){
    fallSpeed[0]=0;
    fallSpeed[2]=0;
    }
  }

void MoveAlgo::setPos(std::array<float,3> pos,uint64_t dt,float speed) {
  float               gravity=3*9.8f;
  std::array<float,3> fb=npc.position();
  std::array<float,3> norm={};

  if(trySlide(fb,norm)){
    setAsSlide(true);
    npc.setAnim(Npc::Slide);
    pos = fb;
    fallSpeed[0]+=0.1f*norm[0];
    fallSpeed[2]+=0.1f*norm[2];
    switch(npc.tryMove(pos,fb,speed)){
      case Npc::MV_FAILED:  pos=npc.position(); break;
      case Npc::MV_CORRECT: pos=fb; break;
      case Npc::MV_OK: break;
      }
    } else {
    setAsSlide(false);
    if(!npc.isFlyAnim() && world.physic()->landNormal(pos[0],pos[1],pos[2])[1]<slideBegin){
      pos = npc.position();
      } else {
      switch(npc.tryMove(pos,fb,speed)){
        case Npc::MV_FAILED:  pos=npc.position(); break;
        case Npc::MV_CORRECT: pos=fb; break;
        case Npc::MV_OK: break;
        }
      }
    }

  bool frozen=(npc.position()==pos) && !isSlide();
  npc.setPosition(pos);

  bool valid   = false;
  bool fallAni = false;
  auto oldY    = pos[1];
  auto ground  = world.physic()->dropRay(pos[0],pos[1],pos[2],valid);
  setAsFrozen(frozen && std::abs(oldY-ground)<0.001f);

  bool nFall=isFaling();
  if(npc.isFlyAnim()) {
    if(oldY>ground){
      pos[1]=oldY;
      } else {
      pos[1]=ground;
      nFall=false;
      }
    } else {
    nFall=false;
    float dY = (pos[1]-ground);
    if(dY<1) {
      fallSpeed[1]=0;
      } else {
      nFall=true;
      fallSpeed[1]+=gravity*(float(dt)/1000.f);
      }

    if(dY>fallThreshold || isFaling())
      fallAni=true;
    if(dY>fallSpeed[1])
      dY=fallSpeed[1];
    pos[0]+=fallSpeed[0];
    pos[1]-=dY;
    pos[2]+=fallSpeed[2];
    }

  switch(npc.tryMove(pos,fb,speed)) {
    case Npc::MV_CORRECT:
    case Npc::MV_OK: // fall anim
      npc.setPosition(fb);
      if(fallAni) {
        setAsFaling(nFall);
        if(!isSlide())
          npc.setAnim(Npc::Fall);
        } else {
        setAsFaling(false);
        }
      break;
    case Npc::MV_FAILED:
      setAsFaling(false);
      break;
    }
  }

bool MoveAlgo::trySlide(std::array<float,3>& pos,std::array<float,3>& norm) {
  if(npc.isFlyAnim())
    return false;

  bool  badFall = false;
  bool  valid   = false;
  auto  ground  = world.physic()->dropRay(pos[0],pos[1],pos[2],valid);
  float delta   = (pos[1]-ground);

  if(valid && delta>fallThreshold){
    std::array<float,3> drop=pos;
    drop[1]-=std::min(fallSpeed[1],delta);
    if(npc.tryMove(drop,drop,0.f)){
      badFall=true;
      return false;
      }
    }

  norm = world.physic()->landNormal(pos[0],pos[1],pos[2]);

  if(badFall || norm[1]<slideBegin) { // sliding
    pos[0]+=fallSpeed[0];
    pos[2]+=fallSpeed[2];
    return true;
    }
  return false;
  }

