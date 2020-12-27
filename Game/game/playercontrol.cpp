#include "playercontrol.h"

#include "world/npc.h"
#include "world/item.h"
#include "world/interactive.h"
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
  const auto ws    = pl->weaponState();
  const bool melle = (ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H);
  if(other==nullptr) {
    if(!(melle && ctrl[Action::ActionGeneric])) {
      // dont lose focus in melee combat
      pl->setTarget(nullptr);
      }
    } else {
    pl->setTarget(other);
    }
  }

void PlayerControl::onKeyPressed(KeyCodec::Action a, Tempest::KeyEvent::KeyType key) {
  auto    w    = world();
  auto    pl   = w  ? w->player() : nullptr;
  auto    ws   = pl ? pl->weaponState() : WeaponState::NoWeapon;
  uint8_t slot = pl ? pl->inventory().currentSpellSlot() : Item::NSLOT;

  if(pl!=nullptr) {
    if(a==Action::Weapon) {
      if(ws!=WeaponState::NoWeapon) //Currently a weapon is active
        wctrl[WeaponClose] = true;
      else {
        if(wctrl_last>=WeponAction::Weapon3 && pl->inventory().currentSpell(static_cast<uint8_t>(wctrl_last-3))==nullptr)
          wctrl_last=WeponAction::WeaponBow;  //Spell no longer available -> fallback to Bow.
        if(wctrl_last==WeponAction::WeaponBow && pl->currentRangeWeapon()==nullptr)
          wctrl_last=WeponAction::WeaponMele; //Bow no longer available -> fallback to Mele.
        wctrl[wctrl_last] = true;
        }
      return;
      }

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

    for(int i=Action::WeaponMage3;i<=Action::WeaponMage10;++i) {
      if(a==i) {
        int id = (i-Action::WeaponMage3+3);
        if(ws==WeaponState::Mage && slot==id)
          wctrl[WeaponClose] = true; else
          wctrl[id         ] = true;
        return;
        }
      }
    if(key==Tempest::KeyEvent::K_Return)
      ctrl[Action::K_ENTER] = true;
    }

  if(ctrl[KeyCodec::ActionGeneric]) {
    int fk = -1;
    if(a==Action::Forward)
      fk = ActForward;
    if(ws==WeaponState::W1H || ws==WeaponState::W2H) {
      if(a==Action::Back)
        fk = ActBack;
      if(a==Action::Left || a==Action::RotateL)
        fk = ActLeft;
      if(a==Action::Right || a==Action::RotateR)
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

  if(a==Action::Walk) {
    toogleWalkMode();
    return;
    }

  if(a==Action::Sneak) {
    toggleSneakMode();
    return;
    }

  ctrl[a] = true;
  }

void PlayerControl::onKeyReleased(KeyCodec::Action a) {
  ctrl[a] = false;

  auto w  = world();
  auto pl = w ? w->player() : nullptr;
  if(a==KeyCodec::Map && pl!=nullptr) {
    w->script().playerHotKeyScreenMap(*pl);
    }
  if(a==KeyCodec::ActionGeneric)
    std::memset(actrl,0,sizeof(actrl));
  }

void PlayerControl::onRotateMouse(float dAngle) {
  dAngle = std::max(-40.f,std::min(dAngle,40.f));
  rotMouse += dAngle*0.3f;
  }

void PlayerControl::onRotateMouseDy(float dAngle) {
  dAngle = std::max(-100.f,std::min(dAngle,100.f));
  rotMouseY += dAngle*0.4f;
  }

void PlayerControl::tickFocus() {
  currentFocus = findFocus(&currentFocus);
  setTarget(currentFocus.npc);

  if(!ctrl[Action::ActionGeneric])
    return;

  auto focus = currentFocus;
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

void PlayerControl::clearFocus() {
  currentFocus = Focus();
  }

void PlayerControl::actionFocus(Npc& other) {
  setTarget(&other);
  }

void PlayerControl::emptyFocus() {
  setTarget(nullptr);
  }

Focus PlayerControl::focus() const {
  return currentFocus;
  }

bool PlayerControl::hasActionFocus() const {
  if(!ctrl[Action::ActionGeneric])
    return false;
  return currentFocus.npc!=nullptr;
  }

bool PlayerControl::interact(Interactive &it) {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return false;
  auto pl = w->player();
  if(pl->weaponState()!=WeaponState::NoWeapon)
    return false;
  if(w->player()->isDown())
    return true;
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
  if(w->player()->isDown())
    return true;
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
  if(w->player()->isDown())
    return true;
  return pl->takeItem(item)!=nullptr;
  }

void PlayerControl::toogleWalkMode() {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return;
  auto pl = w->player();
  pl->setWalkMode(pl->walkMode()^WalkBit::WM_Walk);
  }

void PlayerControl::toggleSneakMode() {
  auto w = world();
  if(w==nullptr || w->player()==nullptr)
    return;
  auto pl = w->player();
  if(pl->canSneak())
    pl->setWalkMode(pl->walkMode()^WalkBit::WM_Sneak);
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

  pos.y+=50;

  pos.x+=60*s;
  pos.z+=-60*c;

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
  const float dtF = float(dt)/1000.f;

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

  float runAngle = pl->runAngle();
  if(runAngle!=0.f || std::fabs(runAngleDest)>5.f) {
    const float speed = 30.f;
    if(runAngle<runAngleDest) {
      runAngle+=speed*dtF;
      if(runAngle>runAngleDest)
        runAngle = runAngleDest;
      pl->setRunAngle(runAngle);
      }
    else if(runAngle>runAngleDest) {
      runAngle-=speed*dtF;
      if(runAngle<runAngleDest)
        runAngle = runAngleDest;
      pl->setRunAngle(runAngle);
      }
    }

  rotMouseY = 0;
  return true;
  }

void PlayerControl::implMove(uint64_t dt) {
  auto  w        = world();
  Npc&  pl       = *w->player();
  float rot      = pl.rotation();
  float rotY     = pl.rotationY();
  auto  gl       = pl.guild();
  float rspeed   = float(w->script().guildVal().turn_speed[gl])*(float(dt)/1000.f)*60.f/100.f;
  auto  ws       = pl.weaponState();

  Npc::Anim ani=Npc::Anim::Idle;

  if((pl.bodyState()&BS_MAX)==BS_DEAD)
    return;
  if((pl.bodyState()&BS_MAX)==BS_UNCONSCIOUS)
    return;

  if(pl.interactive()!=nullptr) {
    if(ctrl[KeyCodec::Back])
      pl.setInteraction(nullptr);
    // animation handled in MOBSI
    return;
    }

  if(!pl.isAiQueueEmpty()) {
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
  if(std::fabs(rotMouse)>0.f) {
    if(rotMouse>0)
      rotation = -1; else
      rotation = 1;
    rot +=rotMouse;
    rotMouse  = 0;
    }
  rotY+=rotMouseY;

  pl.setDirectionY(rotY);
  if(pl.isFaling() || pl.isSlide() || pl.isInAir()){
    pl.setDirection(rot);
    return;
    }

  if(casting) {
    if(actrl[ActForward]) {
      actrl[ActForward] = false;
      } else {
      casting = false;
      pl.endCastSpell();
      }
    return;
    }

  if(ctrl[Action::K_ENTER]) {
    pl.transformBack();
    ctrl[Action::K_ENTER] = false;
    }

  if(pl.canSwitchWeapon()) {
    if(wctrl[WeaponClose]) {
      wctrl[WeaponClose] = !pl.closeWeapon(false);
      return;
      }
    if(wctrl[WeaponMele]) {
      bool ret=false;
      if(pl.currentMeleWeapon()!=nullptr)
        ret = pl.drawWeaponMele(); else
        ret = pl.drawWeaponFist();
      wctrl[WeaponMele] = !ret;
      wctrl_last = WeaponMele;
      return;
      }
    if(wctrl[WeaponBow]) {
      if(pl.currentRangeWeapon()!=nullptr){
        wctrl[WeaponBow] = !pl.drawWeaponBow();
        wctrl_last = WeaponBow;
        } else {
        wctrl[WeaponBow] = false;
        }
      return;
      }
    for(uint8_t i=0;i<8;++i) {
      if(wctrl[Weapon3+i]){
        if(pl.inventory().currentSpell(i)!=nullptr){
          bool ret = pl.drawMage(uint8_t(3+i));
          wctrl[Weapon3+i] = !ret;
          wctrl_last = static_cast<WeponAction>(Weapon3+i);
          if(ret) {
            if(auto spl = pl.inventory().currentSpell(i)) {
              gothic.onPrint(spl->description());
              }
            }
          } else {
          wctrl[Weapon3+i] = false;
          }
        return;
        }
      }
    }

  if((ws==WeaponState::Bow || ws==WeaponState::CBow) && pl.hasAmunition()) {
    if(actrl[ActGeneric] || actrl[ActForward]) {
      if(auto other = pl.target()) {
        auto dp = other->position()-pl.position();
        pl.lookAt(dp.x,dp.z,false,dt);
        pl.aimBow();
        } else
      if(currentFocus.interactive!=nullptr) {
        auto dp = currentFocus.interactive->position()-pl.position();
        pl.lookAt(dp.x,dp.z,false,dt);
        }
      pl.aimBow();
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
        pl.shootBow(currentFocus.interactive);
        return;
        }
      case WeaponState::Mage: {
        casting = pl.beginCastSpell();
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

  if(ctrl[Action::Forward]) {
    if((pl.walkMode()&WalkBit::WM_Dive)!=WalkBit::WM_Dive) {
      ani = Npc::Anim::Move;
      } else if(pl.isDive()) {
      pl.setDirectionY(rotY - 1);
      return;
      }
    }
  else if(ctrl[Action::Back]) {
    if((pl.walkMode()&WalkBit::WM_Dive)!=WalkBit::WM_Dive) {
      ani = Npc::Anim::MoveBack;
      } else if(pl.isDive()) {
      pl.setDirectionY(rotY + 1);
      return;
      }
    }
  else if(ctrl[Action::Left])
    ani = Npc::Anim::MoveL;
  else if(ctrl[Action::Right])
    ani = Npc::Anim::MoveR;

  if(ctrl[Action::Jump]) {
    if(pl.isDive()) {
      ani = Npc::Anim::Move;
      }
    else if(pl.isSwim()) {
      pl.startDive();
      }
    else if(pl.isStanding()) {
      auto code = pl.tryJump(pl.position());
      if(!pl.isFaling() && !pl.isSlide() && code!=Npc::JumpCode::JM_OK){
        pl.startClimb(code);
        return;
        }
      ani = Npc::Anim::Jump;
      }
    else {
      ani = Npc::Anim::Jump;
      }
    }

  if(!pl.isCasting()) {
    if(ani==Npc::Anim::Jump) {
      pl.setAnimRotate(0);
      rotation = 0;
      }
    pl.setAnim(ani);
    }
  setAnimRotate(pl, rot, ani==Npc::Anim::Idle ? rotation : 0, ctrl[KeyCodec::RotateL] || ctrl[KeyCodec::RotateR], dt);
  if(actrl[ActGeneric] || ani==Npc::Anim::MoveL || ani==Npc::Anim::MoveR || pl.isFinishingMove()) {
    if(auto other = pl.target()) {
      if(pl.weaponState()==WeaponState::NoWeapon || other->isDown() || pl.isFinishingMove()){
        pl.setTarget(nullptr);
        } else {
        float dx = other->position().x-pl.position().x;
        float dz = other->position().z-pl.position().z;
        // pl.lookAt(dx,dz,false,dt);
        pl.setDirection(dx,0,dz);
        rot = pl.rotation();
        }
      }
    }

  if(ani==Npc::Anim::Move && (rotation!=0 || rotY!=0)) {
    assignRunAngle(pl,rot,dt);
    } else {
    assignRunAngle(pl,pl.rotation(),dt);
    }
  pl.setDirection(rot);
  }

void PlayerControl::assignRunAngle(Npc& pl, float rotation, uint64_t dt) {
  float dtF    = (float(dt)/1000.f);
  float angle  = pl.rotation();
  float dangle = (rotation-angle)/dtF;
  auto& wrld   = pl.world();

  if(std::fabs(dangle)<5.f) {
    if(runAngleSmooth<wrld.tickCount())
      runAngleDest = 0;
    return;
    }

  if(dangle>0)
    dangle-=5.f;
  if(dangle<0)
    dangle+=5.f;

  dangle *= 0.25f;

  float maxV = 12.5;
  if(angle<rotation)
    runAngleDest =  std::min( dangle,maxV);
  if(angle>rotation)
    runAngleDest = -std::min(-dangle,maxV);
  runAngleSmooth = wrld.tickCount()+150;
  }

void PlayerControl::setAnimRotate(Npc& pl, float rotation, int anim, bool force, uint64_t dt) {
  float dtF    = (float(dt)/1000.f);
  float angle  = pl.rotation();
  float dangle = (rotation-angle)/dtF;
  auto& wrld   = pl.world();

  if(std::fabs(dangle)<100.f && !force) // 100 deg per second threshold
    anim = 0;
  if(rotationAni==anim && anim!=0)
    force = true;
  if(!force && wrld.tickCount()<turnAniSmooth)
    return;
  turnAniSmooth = wrld.tickCount() + 150;
  rotationAni   = anim;
  pl.setAnimRotate(anim);
  }
