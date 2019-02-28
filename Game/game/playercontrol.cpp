#include "playercontrol.h"

#include "world/npc.h"
#include "world/world.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"

#include <cmath>

PlayerControl::PlayerControl(DialogMenu& dlg, InventoryMenu &inv)
  :dlg(dlg),inv(inv) {
  }

void PlayerControl::setWorld(World *w) {
  world = w;
  clearInput();
  }

bool PlayerControl::interact(Interactive &it) {
  if(world==nullptr || world->player()==nullptr)
    return false;
  if(world->player()->weaponState()!=Inventory::WeaponState::NoWeapon)
    return false;
  if(it.isContainer()){
    inv.open(*world->player(),it);
    return true;
    }
  return dlg.start(*world->player(),it);
  }

bool PlayerControl::interact(Npc &other) {
  if(world==nullptr || world->player()==nullptr)
    return false;
  if(world->player()->weaponState()!=Inventory::WeaponState::NoWeapon)
    return false;
  return dlg.start(*world->player(),other);
  }

bool PlayerControl::interact(Item &item) {
  if(world==nullptr || world->player()==nullptr)
    return false;
  if(world->player()->weaponState()!=Inventory::WeaponState::NoWeapon)
    return false;
  std::unique_ptr<Item> ptr {world->takeItem(item)};
  world->player()->addItem(std::move(ptr));
  return true;
  }

void PlayerControl::clearInput() {
  std::memset(ctrl,0,sizeof(ctrl));
  }

void PlayerControl::drawFist() {
  ctrl[DrawFist]=true;
  }

void PlayerControl::drawWeaponMele() {
  ctrl[DrawWeaponMele]=true;
  }

void PlayerControl::drawWeaponBow() {
  ctrl[DrawWeaponBow]=true;
  }

void PlayerControl::drawWeaponMage(uint8_t s) {
  if(s>=3 && s<=10)
    ctrl[DrawWeaponMage3+s-3]=true;
  }

void PlayerControl::action() {
  ctrl[Action]=true;
  }

void PlayerControl::jump() {
  ctrl[Jump]=true;
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

void PlayerControl::actionForward() {
  ctrl[ActForward]=true;
  }

void PlayerControl::actionLeft() {
  ctrl[ActLeft]=true;
  }

void PlayerControl::actionRight() {
  ctrl[ActRight]=true;
  }

void PlayerControl::actionBack() {
  ctrl[ActBack]=true;
  }

void PlayerControl::marvinF8() {
  if(world==nullptr || world->player()==nullptr)
    return;

  auto& pl  = *world->player();
  auto  pos = pl.position();
  float rot = pl.rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);

  pos[1]+=50;

  pos[0]+=60*s;
  pos[2]+=-60*c;

  pl.setPosition(pos);
  pl.clearSpeed();
  pl.setAnim(Npc::Idle);
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
    if(ctrl[RotateL]) {
      rot += rspeed*dt;
      ani  = Npc::Anim::RotL;
      }
    if(ctrl[RotateR]) {
      rot -= rspeed*dt;
      ani  = Npc::Anim::RotR;
      }

    if(pl.isFaling() || pl.isSlide() || pl.isInAir()){
      pl.setDirection(rot);
      return;
      }

    if(ctrl[DrawFist]){
      pl.drawWeaponFist();
      return;
      }
    if(ctrl[DrawWeaponMele]){
      pl.drawWeaponMele();
      return;
      }
    if(ctrl[DrawWeaponBow]){
      pl.drawWeaponBow();
      return;
      }
    if(ctrl[DrawWeaponMage3]){
      pl.drawMage(3);
      return;
      }

    if(ctrl[ActForward]) {
      auto ws = pl.weaponState();
      if(ws==Inventory::WeaponState::W1H ||
         ws==Inventory::WeaponState::W2H) {
        pl.swingSword();
        return;
        }
      else if(ws==Inventory::WeaponState::Mage) {
        if(pl.castSpell())
          return;
        }
      ctrl[Forward]=true;
      }
    if(ctrl[ActLeft] || ctrl[ActRight] || ctrl[ActBack]) {
      auto ws = pl.weaponState();
      if(ws==Inventory::WeaponState::W1H || ws==Inventory::WeaponState::W2H){
        if(ctrl[ActLeft])
          pl.swingSwordL(); else
        if(ctrl[ActRight])
          pl.swingSwordR(); else
        if(ctrl[ActBack])
          pl.blockSword();
        return;
        }
      }

    if(ctrl[Jump]) {
      if(pl.anim()==Npc::Idle){
        switch(pl.tryJump(pl.position())){
          case Npc::JM_Up:
            ani = Npc::Anim::JumpUp;
            break;
          case Npc::JM_UpLow:
            ani = Npc::Anim::JumpUpLow;
            break;
          case Npc::JM_UpMid:
            ani = Npc::Anim::JumpUpMid;
            break;
          case Npc::JM_OK:
            ani = Npc::Anim::Jump;
            break;
          }
        if(!pl.isFaling() && !pl.isSlide() && ani!=Npc::Anim::Jump){
          pl.startClimb(ani);
          return;
          }
        } else {
        ani = Npc::Anim::Jump;
        }
      }
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

  pl.setAnim(ani);
  pl.setDirection(rot);
  }
