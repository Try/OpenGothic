#include "playercontrol.h"

#include "world/npc.h"
#include "world/world.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"
#include "gothic.h"

#include <cmath>

PlayerControl::PlayerControl(Gothic &gothic, DialogMenu& dlg, InventoryMenu &inv)
  :gothic(gothic), dlg(dlg),inv(inv) {
  }

bool PlayerControl::interact(Interactive &it) {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return false;
  if(w->player()->weaponState()!=WeaponState::NoWeapon)
    return false;
  if(it.isContainer()){
    inv.open(*w->player(),it);
    return true;
    }

  if(!w->player()->setInteraction(&it)){

    }
  return true;
  }

bool PlayerControl::interact(Npc &other) {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return false;
  if(w->player()->weaponState()!=WeaponState::NoWeapon)
    return false;
  if(w->script().isDead(other) || w->script().isUnconscious(other)){
    if(!inv.ransack(*w->player(),other))
      w->script().printNothingToGet();
    }
  other.startDialog(*w->player());
  return true;
  }

bool PlayerControl::interact(Item &item) {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return false;
  auto pl = w->player();
  if(pl->weaponState()!=WeaponState::NoWeapon)
    return false;
  std::unique_ptr<Item> ptr {w->takeItem(item)};
  auto it = ptr.get();
  pl->addItem(std::move(ptr));

  if(it->handle()->owner!=0)
    w->sendPassivePerc(*pl,*pl,*pl,*it,Npc::PERC_ASSESSTHEFT);
  return true;
  }

void PlayerControl::invokeMobsiState() {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return;
  auto pl = w->player();
  if(pl==nullptr)
    return;

  auto inter = pl->interactive();
  if(inter==nullptr || mobsiState==inter->stateId())
    return;
  mobsiState = inter->stateId();
  auto st = inter->stateFunc();
  if(!st.empty()) {
    auto& sc = pl->world().script();
    sc.useInteractive(pl->handle(),st);
    }
  }

void PlayerControl::toogleWalkMode() {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return;
  auto pl = w->player();
  pl->setWalkMode(WalkBit(pl->walkMode()^WalkBit::WM_Walk));
  }

WeaponState PlayerControl::weaponState() const {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return WeaponState::NoWeapon;
  auto pl = w->player();
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
  auto w  = world();
  auto ws = weaponState();
  clrDraw();

  auto    pl   = w ? w->player() : nullptr;
  uint8_t slot = pl ? pl->inventory().currentSpellSlot() : Item::NSLOT;
  if(ws==WeaponState::Mage && s==slot) {
    ctrl[CloseWeapon   ]=true;
    } else {
    if(s>=3 && s<=10)
      ctrl[DrawWeaponMage3+s-3]=true;
    }
  }

void PlayerControl::actionFocus(Npc& other) {
  setTarget(&other);
  ctrl[ActionFocus]=true;
  }

void PlayerControl::emptyFocus() {
  ctrl[EmptyFocus]=true;
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

void PlayerControl::rotateMouse(int dAngle) {
  rotMouse    = std::abs(dAngle)>3 ? 40u : rotMouse;
  rotMouseDir = dAngle>0;
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
  auto w  = world();
  auto pl = w ? w->player() : nullptr;
  if(pl==nullptr || pl->isInAnim(Npc::Anim::AtackFinish))
    return;
  auto ws = pl->weaponState();
  if(other!=nullptr || (ws!=WeaponState::W1H && ws!=WeaponState::W2H))
    pl->setTarget(other); // dont lose focus in melee combat
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
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return;

  auto& pl  = *w->player();
  auto  pos = pl.position();
  float rot = pl.rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);

  pos[1]+=50;

  pos[0]+=60*s;
  pos[2]+=-60*c;

  pl.changeAttribute(Npc::ATR_HITPOINTS,pl.attribute(Npc::ATR_HITPOINTSMAX),false);
  pl.clearState(false);
  pl.setPosition(pos);
  pl.clearSpeed();
  pl.setAnim(AnimationSolver::Idle);
  }

Focus PlayerControl::findFocus(Focus* prev,const Camera& camera,int wx, int hx) {
  auto w = world();
  if(w==nullptr)
    return Focus();
  if(!cacheFocus)
    prev = nullptr;

  if(prev)
    return w->findFocus(*prev,camera.view(),wx,hx);
  return w->findFocus(Focus(),camera.view(),wx,hx);
  }

World *PlayerControl::world() const {
  return gothic.world();
  }

bool PlayerControl::tickMove(uint64_t dt) {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return false;
  cacheFocus = ctrl[ActionFocus] || ctrl[ActForward] || ctrl[ActLeft] || ctrl[ActRight] || ctrl[ActBack];
  implMove(dt);
  std::memset(ctrl,0,Walk);
  invokeMobsiState();
  return true;
  }

void PlayerControl::implMove(uint64_t dt) {
  auto  w      = world();
  Npc&  pl     = *w->player();
  float rot    = pl.rotation();
  auto  gl     = std::min<uint32_t>(pl.guild(),GIL_MAX);
  float rspeed = w->script().guildVal().turn_speed[gl]*(dt/1000.f)*60.f/100.f;
  auto  ws     = pl.weaponState();

  Npc::Anim ani=Npc::Anim::Idle;

  if((pl.bodyState()&Npc::BS_MAX)==Npc::BS_DEAD)
    return;
  if((pl.bodyState()&Npc::BS_MAX)==Npc::BS_UNCONSCIOUS)
    return;

  if(!pl.isAiQueueEmpty()) {
    pl.setAnim(Npc::Anim::Idle);
    return;
    }

  int rotation=0;
  if(pl.interactive()==nullptr) {
    if(ctrl[RotateL]) {
      rot += rspeed;
      rotation = -1;
      rotMouse=0;
      }
    if(ctrl[RotateR]) {
      rot -= rspeed;
      rotation = 1;
      rotMouse=0;
      }

    if(rotMouse>0) {
      if(rotMouseDir){
        rot += rspeed;
        rotation = -1;
        } else {
        rot -= rspeed;
        rotation = 1;
        }
      if(rotMouse<dt)
        rotMouse=0; else
        rotMouse-=dt;
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
          if(ws==WeaponState::Mage) {
            if(auto spl = pl.inventory().currentSpell(i)) {
              gothic.print(spl->description());
              }
            }
          } else {
          ctrl[DrawWeaponMage3+i] = false;
          }
        return;
        }
      }

    if((ws==WeaponState::Bow || ws==WeaponState::CBow) &&
       pl.hasAmunition()){
      if(ctrl[ActionFocus]) {
        if(auto other = pl.target()) {
          float dx = other->position()[0]-pl.position()[0];
          float dz = other->position()[2]-pl.position()[2];
          pl.lookAt(dx,dz,false,dt);
          pl.aimBow();
          return;
          }
        }
      if(ctrl[EmptyFocus]){
        pl.aimBow();
        if(!ctrl[ActForward])
          return;
        }
      }

    if(ctrl[ActForward]) {
      auto ws = pl.weaponState();
      if(ws==WeaponState::Fist) {
        pl.fistShoot();
        return;
        }
      if(ws==WeaponState::W1H || ws==WeaponState::W2H) {
        if(pl.target()!=nullptr && pl.canFinish(*pl.target()))
          pl.finishingMove(); else
          pl.swingSword();
        return;
        }
      if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
        pl.shootBow();
        return;
        }
      if(ws==WeaponState::Mage) {
        pl.castSpell();
        return;
        }
      ctrl[ActForward]=true;
      ctrl[Forward]   =true;
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
      if(pl.isInAnim(AnimationSolver::Idle)){
        auto code = pl.tryJump(pl.position());
        if(!pl.isFaling() && !pl.isSlide() && code!=Npc::JumpCode::JM_OK){
          pl.startClimb(code);
          return;
          }
        ani = Npc::Anim::Jump;
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
  pl.setAnimRotate(ani==Npc::Anim::Idle ? rotation : 0);
  if(ctrl[ActionFocus] || ani==Npc::Anim::MoveL || ani==Npc::Anim::MoveR || pl.isInAnim(Npc::Anim::AtackFinish)) {
    if(auto other = pl.target()){
      if((pl.weaponState()==WeaponState::NoWeapon || other->isDown()) && pl.isInAnim(Npc::Anim::AtackFinish)){
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
