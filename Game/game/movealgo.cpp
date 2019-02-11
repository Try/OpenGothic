#include "movealgo.h"

#include "world/world.h"
#include "world/npc.h"

MoveAlgo::MoveAlgo(Npc& unit, const World &w)
  :npc(unit),world(w) {
  }

void MoveAlgo::tick(uint64_t dt) {
  float dpos[3]={};
  auto  dp  = npc.animMoveSpeed(npc.anim(),dt);
  auto  pos = npc.position();
  float rot = npc.rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);

  dpos[0]=dp.x;
  dpos[1]=dp.y;
  dpos[2]=dp.z;

  pos[0]+=mulSpeed*(dpos[0]*c-dpos[2]*s);
  pos[2]+=mulSpeed*(dpos[0]*s+dpos[2]*c);
  if(npc.isFlyAnim())
    pos[1]+=dpos[1];

  pSpeed = std::sqrt(dpos[0]*dpos[0]+dpos[2]*dpos[2]);
  setPos(pos,dt,pSpeed);
  }

void MoveAlgo::setPos(std::array<float,3> pos,uint64_t dt,float speed) {
  float gravity=3*9.8f;

  std::array<float,3> fb;
  switch(npc.tryMove(pos,fb,speed)){
    case Npc::MV_FAILED:  pos=npc.position(); break;
    case Npc::MV_CORRECT: pos=fb; break;
    case Npc::MV_OK: break;
    }
  npc.setPosition(pos);

  bool valid   = false;
  bool fallAni = false;
  auto oldY    = pos[1];
  auto ground  = world.physic()->dropRay(pos[0],pos[1],pos[2],valid);

  if(npc.isFlyAnim()) {
    if(oldY>ground)
      pos[1]=oldY; else
      pos[1]=ground;
    } else {
    float dY = (pos[1]-ground);
    if(dY<1) {
      fallSpeed=0;
      } else {
      fallSpeed+=gravity*(float(dt)/1000.f);
      }
    if(dY>45)
      fallAni=true;
    if(dY>fallSpeed)
      dY=fallSpeed;
    pos[1]-=dY;
    }

  switch(npc.tryMove(pos,fb,speed)) {
    case Npc::MV_CORRECT:
    case Npc::MV_OK:
      npc.setPosition(fb);
      if(fallAni)
        npc.setAnim(Npc::Fall);
      break;
    case Npc::MV_FAILED:
      break;
    }
  }
