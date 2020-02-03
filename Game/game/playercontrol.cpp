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

bool PlayerControl::isInMove() {
  return ctrl[Action::Forward] | ctrl[Action::Left] | ctrl[Action::Right]; // | ctrl[Action::Back];
  }

void PlayerControl::setTarget(Npc *other) {
  auto w  = world();
  auto pl = w ? w->player() : nullptr;
  if(pl==nullptr || pl->isFinishingMove())
    return;
  auto ws = pl->weaponState();
  if(other!=nullptr || (ws!=WeaponState::W1H && ws!=WeaponState::W2H))
    pl->setTarget(other); // dont lose focus in melee combat
  }

void PlayerControl::onKeyPressed(KeyCodec::Action a) {
  auto    w    = world();
  auto    pl   = w  ? w->player() : nullptr;
  auto    ws   = pl ? pl->weaponState() : WeaponState::NoWeapon;
  uint8_t slot = pl ? pl->inventory().currentSpellSlot() : Item::NSLOT;

  if(a==Action::WeaponMele) {
    if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H)
      wctrl[WeaponClose] = true; else
      wctrl[WeaponMele ] = true;
    return;
    }

  if(a==Action::WeaponBow) {
    if(ws==WeaponState::Bow || ws==WeaponState::CBow)
      wctrl[WeaponClose] = true; else
      wctrl[WeaponBow  ] = true;
    return;
    }

  for(int i=Action::WeaponMage3;i<=Action::WeaponMage10;++i){
    if(a==i) {
      int id = (i-Action::WeaponMage3+3);
      if(ws==WeaponState::Mage && slot==id)
        wctrl[WeaponClose] = true; else
        wctrl[id]          = true;
      return;
      }
    }

  if(ctrl[KeyCodec::ActionGeneric]) {
    int fk = -1;
    if(a==Action::Forward)
      fk = ActForward;
    if(ws==WeaponState::W1H || ws==WeaponState::W2H) {
      if(a==Action::Back)
        fk = ActBack;
      if(a==Action::Left)
        fk = ActLeft;
      if(a==Action::Right)
        fk = ActRight;
      }
    if(fk>=0) {
      std::memset(actrl,0,sizeof(actrl));
      actrl[ActGeneric] = true;
      actrl[fk]         = true;

      ctrl[a] = true;
      return;
      }
    }

  if(a==KeyCodec::ActionGeneric) {
    FocusAction fk = ActGeneric;
    if(ctrl[Action::Forward])
      fk = ActForward;
    std::memset(actrl,0,sizeof(actrl));
    actrl[fk] = true;
    ctrl[a] = true;
    return;
    }

  if(a==Action::Walk){
    toogleWalkMode();
    return;
    }

  ctrl[a] = true;
  }

void PlayerControl::onKeyReleased(KeyCodec::Action a) {
  ctrl[a] = false;
  if(a==KeyCodec::ActionGeneric)
    std::memset(actrl,0,sizeof(actrl));
  }

void PlayerControl::onRotateMouse(int dAngle) {
  rotMouse = float(dAngle)*0.4f;
  }

void PlayerControl::tickFocus(Focus& curFocus) {
  curFocus = findFocus(&curFocus);
  setTarget(curFocus.npc);

  if(!ctrl[Action::ActionGeneric])
    return;

  auto focus = curFocus;
  if(focus.interactive!=nullptr && interact(*focus.interactive)) {
    clearInput();
    }
  else if(focus.npc!=nullptr && interact(*focus.npc)) {
    clearInput();
    }
  else if(focus.item!=nullptr && interact(*focus.item)) {
    clearInput();
    }

  if(focus.npc)
    actionFocus(*focus.npc); else
    emptyFocus();
  }

void PlayerControl::actionFocus(Npc& other) {
  setTarget(&other);
  }

void PlayerControl::emptyFocus() {
  setTarget(nullptr);
  }

bool PlayerControl::interact(Interactive &it) {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return false;
  auto pl = w->player();
  if(pl->weaponState()!=WeaponState::NoWeapon)
    return false;
  if(it.isContainer()){
    inv.open(*pl,it);
    return true;
    }
  if(pl->setInteraction(&it)){
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
  std::memset(ctrl, 0,sizeof(ctrl));
  std::memset(actrl,0,sizeof(actrl));
  std::memset(wctrl,0,sizeof(wctrl));
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
  pl.quitIneraction();
  pl.setAnim(AnimationSolver::Idle);
  }

Focus PlayerControl::findFocus(Focus* prev) {
  auto w = world();
  if(w==nullptr)
    return Focus();
  if(!cacheFocus)
    prev = nullptr;

  if(prev)
    return w->findFocus(*prev);
  return w->findFocus(Focus());
  }

World *PlayerControl::world() const {
  return gothic.world();
  }

bool PlayerControl::tickMove(uint64_t dt) {
  auto w = world();
  if(w==nullptr)
    return false;
  Npc*  pl     = w->player();
  auto  camera = gothic.gameCamera();
  if(pl==nullptr) {
    if(camera==nullptr)
      return false;
    if(ctrl[KeyCodec::RotateL])
      camera->rotateLeft();
    if(ctrl[KeyCodec::RotateR])
      camera->rotateRight();
    if(ctrl[KeyCodec::Left])
      camera->moveLeft();
    if(ctrl[KeyCodec::Right])
      camera->moveRight();
    if(ctrl[KeyCodec::Forward])
      camera->moveForward();
    if(ctrl[KeyCodec::Back])
      camera->moveBack();
    return true;
    }

  if(ctrl[Action::K_F8])
    marvinF8();
  cacheFocus = ctrl[Action::ActionGeneric];

  if(ctrl[Action::ActionGeneric] && ctrl[Action::Forward]) {
    actrl[ActForward] = true;
    }
  if(ctrl[Action::ActionGeneric] && ctrl[Action::Back]) {
    actrl[ActBack] = true;
    }

  implMove(dt);
  return true;
  }

void PlayerControl::implMove(uint64_t dt) {
  auto  w        = world();
  Npc&  pl       = *w->player();
  float rot      = pl.rotation();
  auto  gl       = pl.guild();
  float rspeed   = float(w->script().guildVal().turn_speed[gl])*(float(dt)/1000.f)*60.f/100.f;
  auto  ws       = pl.weaponState();

  Npc::Anim ani=Npc::Anim::Idle;

  if((pl.bodyState()&BS_MAX)==BS_DEAD)
    return;
  if((pl.bodyState()&BS_MAX)==BS_UNCONSCIOUS)
    return;

  if(!pl.isAiQueueEmpty()) {
    pl.setAnim(Npc::Anim::Idle);
    return;
    }

  if(pl.interactive()!=nullptr) {
    if(ctrl[KeyCodec::Back])
      pl.setInteraction(nullptr);
    // animation handled in MOBSI
    return;
    }

  int rotation=0;
  if(ctrl[KeyCodec::RotateL]) {
    rot += rspeed;
    rotation = -1;
    rotMouse=0;
    }
  if(ctrl[KeyCodec::RotateR]) {
    rot -= rspeed;
    rotation = 1;
    rotMouse=0;
    }
  if(std::fabs(rotMouse)>5) {
    /* TODO: proper anim
    if(rotMouse>0)
      rotation = -1; else
      rotation = 1; */
    }
  rot+=rotMouse;

  if(pl.isFaling() || pl.isSlide() || pl.isInAir()){
    pl.setDirection(rot);
    return;
    }

  if(wctrl[WeaponClose]) {
    pl.closeWeapon(false);
    wctrl[WeaponClose] = !(weaponState()==WeaponState::NoWeapon);
    return;
    }
  if(wctrl[WeaponMele]) {
    if(pl.currentMeleWeapon()!=nullptr)
      pl.drawWeaponMele(); else
      pl.drawWeaponFist();
    auto ws = weaponState();
    wctrl[WeaponMele] = !(ws==WeaponState::W1H || ws==WeaponState::W2H || ws==WeaponState::Fist);
    return;
    }
  if(wctrl[WeaponBow]) {
    if(pl.currentRangeWeapon()!=nullptr){
      pl.drawWeaponBow();
      auto ws = weaponState();
      wctrl[WeaponBow] = !(ws==WeaponState::Bow || ws==WeaponState::CBow);
      } else {
      wctrl[WeaponBow] = false;
      }
    return;
    }
  for(uint8_t i=0;i<8;++i) {
    if(wctrl[Weapon3+i]){
      if(pl.inventory().currentSpell(i)!=nullptr){
        pl.drawMage(uint8_t(3+i));
        auto ws = weaponState();
        wctrl[Weapon3+i] = !(ws==WeaponState::Mage);
        if(ws==WeaponState::Mage) {
          if(auto spl = pl.inventory().currentSpell(i)) {
            gothic.print(spl->description());
            }
          }
        } else {
        wctrl[Weapon3+i] = false;
        }
      return;
      }
    }

  if((ws==WeaponState::Bow || ws==WeaponState::CBow) && pl.hasAmunition()) {
    if(actrl[ActGeneric]) {
      if(auto other = pl.target()) {
        float dx = other->position()[0]-pl.position()[0];
        float dz = other->position()[2]-pl.position()[2];
        pl.lookAt(dx,dz,false,dt);
        pl.aimBow();
        } else {
        pl.aimBow();
        }
      if(!actrl[ActForward])
        return;
      }
    }

  if(actrl[ActForward]) {
    actrl[ActForward] = false;
    switch(ws) {
      case WeaponState::NoWeapon:
        break;
      case WeaponState::Fist:
        pl.fistShoot();
        return;
      case WeaponState::W1H:
      case WeaponState::W2H: {
        if(pl.target()!=nullptr && pl.canFinish(*pl.target()))
          pl.finishingMove(); else
          pl.swingSword();
        return;
        }
      case WeaponState::Bow:
      case WeaponState::CBow: {
        if(pl.shootBow())
          return;
        break;
        }
      case WeaponState::Mage: {
        if(pl.castSpell())
          return;
        break;
        }
      }
    }

  if(actrl[ActLeft] || actrl[ActRight] || actrl[ActBack]) {
    auto ws = pl.weaponState();
    if(ws==WeaponState::Fist){
      if(actrl[ActBack])
        pl.blockFist();
      return;
      }
    else if(ws==WeaponState::W1H || ws==WeaponState::W2H){
      if(actrl[ActLeft])
        pl.swingSwordL(); else
      if(actrl[ActRight])
        pl.swingSwordR(); else
      if(actrl[ActBack])
        pl.blockSword();
      actrl[ActLeft]  = false;
      actrl[ActRight] = false;
      actrl[ActBack]  = false;
      return;
      }
    }

  if(ctrl[Action::Jump]) {
    if(pl.isStanding()) {
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
  else if(ctrl[Action::Forward])
    ani = Npc::Anim::Move;
  else if(ctrl[Action::Back])
    ani = Npc::Anim::MoveBack;
  else if(ctrl[Action::Left])
    ani = Npc::Anim::MoveL;
  else if(ctrl[Action::Right])
    ani = Npc::Anim::MoveR;

  pl.setAnim(ani);
  pl.setAnimRotate(ani==Npc::Anim::Idle ? rotation : 0);
  if(actrl[ActGeneric] || ani==Npc::Anim::MoveL || ani==Npc::Anim::MoveR || pl.isFinishingMove()) {
    if(auto other = pl.target()){
      if(pl.weaponState()==WeaponState::NoWeapon || other->isDown() || pl.isFinishingMove()){
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
