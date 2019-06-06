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
  if(world->player()->weaponState()!=WeaponState::NoWeapon)
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
  if(world->player()->weaponState()!=WeaponState::NoWeapon)
    return false;
  if(world->script()->isDead(other) || world->script()->isUnconscious(other)){
    if(!inv.ransack(*world->player(),other))
      world->script()->printNothingToGet();
    }
  return dlg.start(*world->player(),other);
  }

bool PlayerControl::interact(Item &item) {
  if(world==nullptr || world->player()==nullptr)
    return false;
  if(world->player()->weaponState()!=WeaponState::NoWeapon)
    return false;
  std::unique_ptr<Item> ptr {world->takeItem(item)};
  world->player()->addItem(std::move(ptr));
  return true;
  }

void PlayerControl::toogleWalkMode() {
  if(world==nullptr || world->player()==nullptr)
    return;
  auto pl = world->player();
  pl->setWalkMode(WalkBit(pl->walkMode()^WalkBit::WM_Walk));
  }

WeaponState PlayerControl::weaponState() const {
  if(world==nullptr || world->player()==nullptr)
    return WeaponState::NoWeapon;
  auto pl = world->player();
  return pl->weaponState();
  }

void PlayerControl::clearInput() {
  std::memset(ctrl,0,sizeof(ctrl));
  }

void PlayerControl::clrDraw() {
  ctrl[DrawWeaponMele]=false;
  ctrl[DrawWeaponBow] =false;
  for(int i=0;i<8;++i)
    ctrl[DrawWeaponMage3+i]=false;
  }

void PlayerControl::drawWeaponMele() {
  auto ws=weaponState();
  clrDraw();
  if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H)
    ctrl[CloseWeapon   ]=true; else
    ctrl[DrawWeaponMele]=true;
  }

void PlayerControl::drawWeaponBow() {
  auto ws=weaponState();
  clrDraw();
  if(ws==WeaponState::Bow || ws==WeaponState::CBow)
    ctrl[CloseWeapon   ]=true; else
    ctrl[DrawWeaponBow]=true;
  }

void PlayerControl::drawWeaponMage(uint8_t s) {
  auto ws=weaponState();
  clrDraw();
  auto    pl   = world ? world->player() : nullptr;
  uint8_t slot = pl ? pl->inventory().currentSpellSlot() : Item::NSLOT;
  if(ws==WeaponState::Mage && s==slot) {
    ctrl[CloseWeapon   ]=true;
    } else {
    if(s>=3 && s<=10)
      ctrl[DrawWeaponMage3+s-3]=true;
    }
  }

void PlayerControl::actionFocus(Npc& other) {
  Npc* pl = world ? world->player() : nullptr;
  if(pl!=nullptr)
    pl->setTarget(&other);
  ctrl[ActionFocus]=true;
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

void PlayerControl::setTarget(Npc *other) {
  if(world==nullptr || world->player()==nullptr)
    return;
  auto ws = weaponState();
  if(other!=nullptr || (ws!=WeaponState::W1H && ws!=WeaponState::W2H))
    world->player()->setTarget(other); // dont lose focus in melee combat
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

  pl.changeAttribute(Npc::ATR_HITPOINTS,pl.attribute(Npc::ATR_HITPOINTSMAX));
  pl.clearState(false);
  pl.setPosition(pos);
  pl.clearSpeed();
  pl.setAnim(AnimationSolver::Idle);
  }

Focus PlayerControl::findFocus(Focus* prev,const Camera& camera,int w, int h) {
  if(world==nullptr)
    return Focus();
  if(!cacheFocus)
    prev = nullptr;

  if(prev)
    return world->findFocus(*prev,camera.view(),w,h);
  return world->findFocus(Focus(),camera.view(),w,h);
  }

bool PlayerControl::tickMove(uint64_t dt) {
  if(world==nullptr || world->player()==nullptr)
    return false;
  cacheFocus = ctrl[ActionFocus] || ctrl[ActForward] || ctrl[ActLeft] || ctrl[ActRight] || ctrl[ActBack];
  implMove(dt);
  std::memset(ctrl,0,Walk);
  return true;
  }

void PlayerControl::implMove(uint64_t dt) {
  Npc&  pl     = *world->player();
  float rot    = pl.rotation();
  auto  gl     = std::min<uint32_t>(pl.guild(),GIL_MAX);
  float rspeed = world->script()->guildVal().turn_speed[gl]*(dt/1000.f)*60.f/100.f;

  Npc::Anim ani=Npc::Anim::Idle;

  if((pl.bodyState()&Npc::BS_MAX)==Npc::BS_DEAD)
    return;
  if((pl.bodyState()&Npc::BS_MAX)==Npc::BS_UNCONSCIOUS)
    return;

  if(pl.interactive()==nullptr) {
    if(ctrl[RotateL]) {
      rot += rspeed;
      ani  = Npc::Anim::RotL;
      }
    if(ctrl[RotateR]) {
      rot -= rspeed;
      ani  = Npc::Anim::RotR;
      }

    if(pl.isFaling() || pl.isSlide() || pl.isInAir()){
      pl.setDirection(rot);
      return;
      }

    if(ctrl[CloseWeapon]){
      pl.closeWeapon(false);
      ctrl[CloseWeapon] = !(weaponState()==WeaponState::NoWeapon);
      return;
      }
    if(ctrl[DrawWeaponMele]) {
      if(pl.currentMeleWeapon()!=nullptr)
        pl.drawWeaponMele(); else
        pl.drawWeaponFist();
      auto ws = weaponState();
      ctrl[DrawWeaponMele] = !(ws==WeaponState::W1H || ws==WeaponState::W2H || ws==WeaponState::Fist);
      return;
      }
    if(ctrl[DrawWeaponBow]){
      if(pl.currentRangeWeapon()!=nullptr){
        pl.drawWeaponBow();
        auto ws = weaponState();
        ctrl[DrawWeaponBow] = !(ws==WeaponState::Bow || ws==WeaponState::CBow);
        } else {
        ctrl[DrawWeaponBow] = false;
        }
      return;
      }
    for(uint8_t i=0;i<8;++i){
      if(ctrl[DrawWeaponMage3+i]){
        if(pl.inventory().currentSpell(i)!=nullptr){
          pl.drawMage(uint8_t(3+i));
          auto ws = weaponState();
          ctrl[DrawWeaponMage3+i] = !(ws==WeaponState::Mage);
          } else {
          ctrl[DrawWeaponMage3+i] = false;
          }
        return;
        }
      }

    if(ctrl[ActionFocus]){
      if(auto other = pl.target()) {
        if(pl.weaponState()==WeaponState::Bow){
          float dx = other->position()[0]-pl.position()[0];
          float dz = other->position()[2]-pl.position()[2];
          pl.lookAt(dx,dz,false,dt);
          pl.aimBow();
          return;
          }
        }
      }
    if(ctrl[ActForward]) {
      auto ws = pl.weaponState();
      if(ws==WeaponState::Fist) {
        pl.fistShoot();
        return;
        }
      if(ws==WeaponState::W1H || ws==WeaponState::W2H) {
        pl.swingSword();
        return;
        }
      if(ws==WeaponState::Bow) {
        if(pl.shootBow())
          return;
        }
      if(ws==WeaponState::Mage) {
        if(pl.castSpell())
          return;
        }
      ctrl[Forward]=true;
      }
    if(ctrl[ActLeft] || ctrl[ActRight] || ctrl[ActBack]) {
      auto ws = pl.weaponState();
      if(ws==WeaponState::Fist){
        if(ctrl[ActBack])
          pl.blockFist();
        return;
        }
      else if(ws==WeaponState::W1H || ws==WeaponState::W2H){
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
      if(pl.anim()==AnimationSolver::Idle){
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
  if(ctrl[ActionFocus] || ani==Npc::Anim::MoveL || ani==Npc::Anim::MoveR){
    if(auto other = pl.target()){
      if(pl.weaponState()==WeaponState::NoWeapon || other->isDown()){
        pl.setOther(nullptr);
        } else {
        float dx = other->position()[0]-pl.position()[0];
        float dz = other->position()[2]-pl.position()[2];
        pl.lookAt(dx,dz,false,dt);
        rot = pl.rotation();
        }
      }
    }
  pl.setDirection(rot);
  }
