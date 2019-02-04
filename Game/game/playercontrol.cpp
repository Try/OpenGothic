#include "playercontrol.h"

#include "world/npc.h"
#include "world/world.h"
#include <cmath>

PlayerControl::PlayerControl() {
  }

void PlayerControl::setWorld(const World *w) {
  world = w;
  }

void PlayerControl::rotateLeft() {
  ctrl[RotateL]=true;
  }

void PlayerControl::rotateRight() {
  ctrl[RotateR]=true;
  }

void PlayerControl::moveForward() {
  ctrl[Forward]=true;
  }

void PlayerControl::moveBack() {
  ctrl[Back]=true;
  }

void PlayerControl::moveLeft() {
  ctrl[Left]=true;
  }

void PlayerControl::moveRight() {
  ctrl[Right]=true;
  }

bool PlayerControl::tickMove(uint64_t dt) {
  if(world==nullptr || world->player()==nullptr)
    return false;
  implMove(dt);
  std::memset(ctrl,0,sizeof(ctrl));
  return true;
  }

void PlayerControl::implMove(uint64_t dt) {
  Npc&  pl = *world->player();
  float rspeed=3.f;

  float k = float(M_PI/180.0), rot = pl.rotation();
  float s=std::sin(rot*k), c=std::cos(rot*k);

  auto      pos=pl.position();
  Npc::Anim ani=Npc::Anim::Idle;
  float     dpos[2]={};

  if(ctrl[RotateL] || ctrl[RotateR]){
    if(ctrl[RotateL]) {
      rot += rspeed;
      ani = Npc::Anim::RotL;
      }
    if(ctrl[RotateR]) {
      rot -= rspeed;
      ani = Npc::Anim::RotR;
      }
    }
  if(ctrl[Forward]) {
    auto dp = pl.animMoveSpeed(Npc::Anim::Move,dt);
    dpos[0]=dp.x;
    dpos[1]=dp.z;
    ani = Npc::Anim::Move;
    }
  else if(ctrl[Back]){
    auto dp = pl.animMoveSpeed(Npc::Anim::MoveBack,dt);
    dpos[0]=dp.x;
    dpos[1]=dp.z;
    ani = Npc::Anim::MoveBack;
    }
  else if(ctrl[Left]) {
    auto dp = pl.animMoveSpeed(Npc::Anim::MoveL,dt);
    dpos[0]=dp.x;
    dpos[1]=dp.z;
    ani = Npc::Anim::MoveL;
    }
  else if(ctrl[Right]) {
    auto dp = pl.animMoveSpeed(Npc::Anim::MoveR,dt);
    dpos[0]=dp.x;
    dpos[1]=dp.z;
    ani = Npc::Anim::MoveR;
    }
  pos[0]+=mulSpeed*(dpos[0]*c-dpos[1]*s);
  pos[2]+=mulSpeed*(dpos[0]*s+dpos[1]*c);

  pSpeed = std::sqrt(dpos[0]*dpos[0]+dpos[1]*dpos[1]);

  bool valid=false;
  pos[1] = world->physic()->dropRay(pos[0],pos[1],pos[2],valid);
  if(valid)
    pl.setPosition(pos);

  pl.setDirection(rot);
  pl.setAnim(ani);
  }
