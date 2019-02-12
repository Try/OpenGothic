#include "playercontrol.h"

#include "world/npc.h"
#include "world/world.h"
#include <cmath>

PlayerControl::PlayerControl() {
  }

bool PlayerControl::interact(Interactive &it) {
  if(world==nullptr || world->player()==nullptr)
    return false;
  return world->player()->setInteraction(&it);
  }

void PlayerControl::drawFist() {
  ctrl[DrawFist]=true;
  }

void PlayerControl::drawWeapon1H() {
  ctrl[DrawWeapon1h]=true;
  }

void PlayerControl::drawWeapon2H() {
  ctrl[DrawWeapon2h]=true;
  }

void PlayerControl::drawWeaponBow() {
  ctrl[DrawWeaponBow]=true;
  }

void PlayerControl::drawWeaponCBow() {
  ctrl[DrawWeaponCBow]=true;
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

void PlayerControl::marvinF8() {
  if(world==nullptr || world->player()==nullptr)
    return;

  auto& pl  = *world->player();
  auto  pos = pl.position();
  pos[1]+=100;
  pl.setPosition(pos);
  }

bool PlayerControl::tickMove(uint64_t dt) {
  if(world==nullptr || world->player()==nullptr)
    return false;
  implMove(dt);
  std::memset(ctrl,0,sizeof(ctrl));
  return true;
  }

void PlayerControl::implMove(uint64_t dt) {
  Npc&  pl     = *world->player();
  float rot    = pl.rotation();
  float rspeed = 90.f/1000.f;

  Npc::Anim ani=Npc::Anim::Idle;

  if(pl.interactive()==nullptr) {
    if(ctrl[DrawFist]){
      pl.drawWeaponFist();
      return;
      }
    if(ctrl[DrawWeapon1h]){
      pl.drawWeapon1H();
      return;
      }
    if(ctrl[DrawWeapon2h]){
      pl.drawWeapon2H();
      return;
      }
    if(ctrl[DrawWeaponBow]){
      pl.drawWeaponBow();
      return;
      }
    if(ctrl[DrawWeaponCBow]){
      pl.drawWeaponCBow();
      return;
      }

    if(ctrl[RotateL]) {
      rot += rspeed*dt;
      ani  = Npc::Anim::RotL;
      }
    if(ctrl[RotateR]) {
      rot -= rspeed*dt;
      ani  = Npc::Anim::RotR;
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
    } else {
    if(ctrl[Back]) {
      ani = Npc::Anim::MoveBack;
      pl.setInteraction(nullptr);
      }
    ani = Npc::Anim::Interact;
    }

  if(!pl.isFaling() && !pl.isSlide()){
    if(pl.isInAir())
      pl.setAnim(Npc::Fall); else
      pl.setAnim(ani);
    }
  pl.setDirection(rot);
  }
