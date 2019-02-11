#include "movealgo.h"

#include "world/world.h"
#include "world/npc.h"

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

bool MoveAlgo::isFrozen() const {
  return flags&Frozen;
  }

void MoveAlgo::setAsFaling(bool f) {
  if(isFaling()==f)
    return;
  if(f) {
    flags=Flags(flags|Faling);
    } else {
    flags=Flags(flags&(~Faling));
    fallSpeed=0;
    }
  }

void MoveAlgo::setAsFrozen(bool f) {
  if(f)
    flags=Flags(flags|Frozen); else
    flags=Flags(flags&(~Frozen));
  }

void MoveAlgo::setPos(std::array<float,3> pos,uint64_t dt,float speed) {
  float gravity=3*9.8f;

  std::array<float,3> fb;
  switch(npc.tryMove(pos,fb,speed)){
    case Npc::MV_FAILED:  pos=npc.position(); break;
    case Npc::MV_CORRECT: pos=fb; break;
    case Npc::MV_OK: break;
    }
  bool frozen=npc.position()==pos;
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
      fallSpeed=0;
      } else {
      nFall=true;
      fallSpeed+=gravity*(float(dt)/1000.f);
      }
    if(dY>45 || isFaling())
      fallAni=true;
    if(dY>fallSpeed)
      dY=fallSpeed;
    pos[1]-=dY;
    }

  switch(npc.tryMove(pos,fb,speed)) {
    case Npc::MV_CORRECT:
    case Npc::MV_OK:
      npc.setPosition(fb);
      if(fallAni) {
        setAsFaling(nFall);
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
