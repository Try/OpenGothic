#include "playercontrol.h"

#include "world/npc.h"
#include "world/world.h"
#include <cmath>

PlayerControl::PlayerControl() {
  }

void PlayerControl::drawFist() {
  ctrl[DrawFist]=true;
  }

void PlayerControl::drawWeapon() {
  ctrl[DrawWeapon1h]=true;
  }

void PlayerControl::drawWeapon2h() {
  ctrl[DrawWeapon2h]=true;
  }

void PlayerControl::drawWeaponMage() {
  ctrl[DrawWeaponMage]=true;
  }

void PlayerControl::action() {
  ctrl[Action]=true;
  }

void PlayerControl::jump() {
  ctrl[Jump]=true;
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

  if(ctrl[DrawFist]){
    pl.drawWeaponFist();
    return;
    }
  if(ctrl[DrawWeapon1h]){
    pl.drawWeaponMelee();
    return;
    }
  if(ctrl[DrawWeapon2h]){
    pl.drawWeapon2H();
    return;
    }

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

  if(ctrl[Jump])
    ani = Npc::Anim::Jump;
  else if(ctrl[Forward])
    ani = Npc::Anim::Move;
  else if(ctrl[Back])
    ani = Npc::Anim::MoveBack;
  else if(ctrl[Left])
    ani = Npc::Anim::MoveL;
  else if(ctrl[Right])
    ani = Npc::Anim::MoveR;

  pl.setAnim(ani);
  auto dp = pl.animMoveSpeed(pl.anim(),dt);
  dpos[0]=dp.x;
  dpos[1]=dp.z;

  pos[0]+=mulSpeed*(dpos[0]*c-dpos[1]*s);
  pos[2]+=mulSpeed*(dpos[0]*s+dpos[1]*c);

  pSpeed = std::sqrt(dpos[0]*dpos[0]+dpos[1]*dpos[1]);

  bool valid  = false;
  auto oldY   = pos[1];
  auto ground = world->physic()->dropRay(pos[0],pos[1],pos[2],valid);
  if(oldY>pos[1] && pl.isFlyAnim()) {
    pos[1]=oldY;
    } else {
    float dY = (pos[1]-ground);
    if(dY<1)
      fallSpeed=0; else
      fallSpeed+=(100*dt)/1000.f;

    if(dY>fallSpeed)
      dY=fallSpeed;
    pos[1]-=dY;
    }

  if(valid)
    pl.setPosition(pos);
  pl.setDirection(rot);
  }
