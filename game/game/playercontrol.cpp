#include "playercontrol.h"

#include <cmath>

#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/objects/interactive.h"
#include "world/world.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"
#include "gothic.h"

PlayerControl::PlayerControl(DialogMenu& dlg, InventoryMenu &inv)
  :dlg(dlg),inv(inv) {
  }

bool PlayerControl::isInMove() {
  return ctrl[Action::Forward] | ctrl[Action::Left] | ctrl[Action::Right]; // | ctrl[Action::Back];
  }

void PlayerControl::setTarget(Npc *other) {
  auto w  = Gothic::inst().world();
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
  auto    w    = Gothic::inst().world();
  auto    pl   = w  ? w->player() : nullptr;
  auto    ws   = pl ? pl->weaponState() : WeaponState::NoWeapon;
  uint8_t slot = pl ? pl->inventory().currentSpellSlot() : Item::NSLOT;

  if(pl!=nullptr) {
    if(a==Action::Weapon) {
      if(ws!=WeaponState::NoWeapon) //Currently a weapon is active
        wctrl[WeaponClose] = true;
      else {
        if(wctrlLast>=WeaponAction::Weapon3 && pl->inventory().currentSpell(static_cast<uint8_t>(wctrlLast-3))==nullptr)
          wctrlLast=WeaponAction::WeaponBow;  //Spell no longer available -> fallback to Bow.
        if(wctrlLast==WeaponAction::WeaponBow && pl->currentRangeWeapon()==nullptr)
          wctrlLast=WeaponAction::WeaponMele; //Bow no longer available -> fallback to Mele.
        wctrl[wctrlLast] = true;
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

  // this odd behaviour is from original game, seem more like a bug
  const bool actTunneling = (pl!=nullptr && pl->isAtackAnim());
  if(ctrl[KeyCodec::ActionGeneric] || actTunneling) {
    int fk = -1;
    if(a==Action::Forward)
      fk = ActForward;
    if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H) {
      if(a==Action::Back)
        fk = ActBack;
      if(a==Action::Left || a==Action::RotateL)
        fk = ActLeft;
      if(a==Action::Right || a==Action::RotateR)
        fk = ActRight;
      }
    if(fk>=0) {
      std::memset(actrl,0,sizeof(actrl));
      actrl[ActGeneric] = ctrl[KeyCodec::ActionGeneric];
      actrl[fk]         = true;

      ctrl[a] = true;
      return;
      }
    }

  if(a==KeyCodec::ActionGeneric) {
    FocusAction fk = ActGeneric;
    if(ctrl[Action::Forward])
      fk = ActMove;
    std::memset(actrl,0,sizeof(actrl));
    actrl[fk] = true;
    ctrl[a]   = true;
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

  if(a==Action::FirstPerson) {
    if(auto c = Gothic::inst().camera())
      c->setFirstPerson(!c->isFirstPerson());
    return;
    }

  ctrl[a] = true;
  }

void PlayerControl::onKeyReleased(KeyCodec::Action a) {
  ctrl[a] = false;

  auto w  = Gothic::inst().world();
  auto pl = w ? w->player() : nullptr;
  if(a==KeyCodec::Map && pl!=nullptr) {
    w->script().playerHotKeyScreenMap(*pl);
    }

  std::memset(actrl,0,sizeof(actrl));
  }

bool PlayerControl::isPressed(KeyCodec::Action a) const {
  return ctrl[a];
  }

void PlayerControl::onRotateMouse(float dAngle) {
  dAngle = std::max(-40.f,std::min(dAngle,40.f));
  rotMouse += dAngle*0.3f;
  }

void PlayerControl::onRotateMouseDy(float dAngle) {
  dAngle = std::max(-100.f,std::min(dAngle,100.f));
  rotMouseY += dAngle*0.2f;
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
  auto w = Gothic::inst().world();
  if(w==nullptr || w->player()==nullptr)
    return false;
  auto pl = w->player();
  if(w->player()->isDown())
    return true;
  if(!canInteract())
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
  auto w = Gothic::inst().world();
  if(w==nullptr || w->player()==nullptr)
    return false;
  auto pl = w->player();
  if(pl->isDown())
    return true;
  if(!canInteract())
    return false;
  if(w->script().isDead(other) || w->script().isUnconscious(other)){
    if(!inv.ransack(*w->player(),other))
      w->script().printNothingToGet();
    }
  if((pl->bodyStateMasked()&BS_MAX)!=BS_NONE)
    return false;
  other.startDialog(*pl);
  return true;
  }

bool PlayerControl::interact(Item &item) {
  auto w = Gothic::inst().world();
  if(w==nullptr || w->player()==nullptr)
    return false;
  auto pl = w->player();
  if(item.isTorchBurn() && pl->isUsingTorch())
    return false;
  if(pl->isDown())
    return true;
  if(!canInteract())
    return false;
  return pl->takeItem(item)!=nullptr;
  }

void PlayerControl::toogleWalkMode() {
  auto w = Gothic::inst().world();
  if(w==nullptr || w->player()==nullptr)
    return;
  auto pl = w->player();
  pl->setWalkMode(pl->walkMode()^WalkBit::WM_Walk);
  }

void PlayerControl::toggleSneakMode() {
  auto w = Gothic::inst().world();
  if(w==nullptr || w->player()==nullptr)
    return;
  auto pl = w->player();
  if(pl->canSneak())
    pl->setWalkMode(pl->walkMode()^WalkBit::WM_Sneak);
  }

WeaponState PlayerControl::weaponState() const {
  auto w = Gothic::inst().world();
  if(w==nullptr || w->player()==nullptr)
    return WeaponState::NoWeapon;
  auto pl = w->player();
  return pl->weaponState();
  }

bool PlayerControl::canInteract() const {
  auto w = Gothic::inst().world();
  if(w==nullptr || w->player()==nullptr)
    return false;
  auto pl = w->player();
  if(pl->weaponState()!=WeaponState::NoWeapon || pl->isAiBusy())
    return false;
  return true;
  }

void PlayerControl::clearInput() {
  std::memset(ctrl, 0,sizeof(ctrl));
  std::memset(actrl,0,sizeof(actrl));
  std::memset(wctrl,0,sizeof(wctrl));
  }

void PlayerControl::marvinF8(uint64_t dt) {
  auto w = Gothic::inst().world();
  if(w==nullptr || w->player()==nullptr)
    return;

  auto& pl  = *w->player();
  auto  pos = pl.position();
  float rot = pl.rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);

  Tempest::Vec3 dp(s,0.8f,-c);
  pos += dp*6000*float(dt)/1000.f;

  pl.changeAttribute(ATR_HITPOINTS,pl.attribute(ATR_HITPOINTSMAX),false);
  pl.changeAttribute(ATR_MANA,     pl.attribute(ATR_MANAMAX),     false);
  pl.clearState(false);
  pl.setPosition(pos);
  pl.clearSpeed();
  pl.quitIneraction();
  pl.setAnim(AnimationSolver::Idle);
  }

void PlayerControl::marvinK(uint64_t dt) {
  auto w = Gothic::inst().world();
  if (w == nullptr || w->player() == nullptr)
    return;

  auto& pl = *w->player();
  auto  pos = pl.position();
  float rot = pl.rotationRad();
  float s = std::sin(rot), c = std::cos(rot);

  Tempest::Vec3 dp(s, 0.0f, -c);
  pos += dp * 6000 * float(dt) / 1000.f;

  pl.clearState(false);
  pl.setPosition(pos);
  pl.clearSpeed();
  pl.quitIneraction();
  // pl.setAnim(AnimationSolver::Idle); // Original G2 behaviour: K doesn't stop running
  }

Focus PlayerControl::findFocus(Focus* prev) {
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return Focus();
  if(!cacheFocus)
    prev = nullptr;

  if(prev)
    return w->findFocus(*prev);
  return w->findFocus(Focus());
  }

bool PlayerControl::tickMove(uint64_t dt) {
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return false;
  const float dtF = float(dt)/1000.f;

  Npc*  pl     = w->player();
  auto  camera = Gothic::inst().camera();
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

  if(ctrl[Action::K_F8] && Gothic::inst().isMarvinEnabled())
    marvinF8(dt);
  if(ctrl[Action::K_K] && Gothic::inst().isMarvinEnabled())
    marvinK(dt);
  cacheFocus = ctrl[Action::ActionGeneric];
  if(camera!=nullptr)
    camera->setLookBack(ctrl[Action::LookBack]);
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
  auto  w        = Gothic::inst().world();
  Npc&  pl       = *w->player();
  float rot      = pl.rotation();
  float rotY     = pl.rotationY();
  auto  gl       = pl.guild();
  float rspeed   = float(w->script().guildVal().turn_speed[gl])*(float(dt)/1000.f)*60.f/100.f;
  auto  ws       = pl.weaponState();
  bool  allowRot = !(pl.isPrehit() || pl.isFinishingMove() || pl.bodyStateMasked()==BS_CLIMB);

  Npc::Anim ani=Npc::Anim::Idle;

  if(pl.bodyStateMasked()==BS_DEAD)
    return;
  if(pl.bodyStateMasked()==BS_UNCONSCIOUS)
    return;

  if(pl.interactive()!=nullptr) {
    implMoveMobsi(pl,dt);
    return;
    }

  if(!pl.isAiQueueEmpty()) {
    runAngleDest = 0;
    return;
    }

  if(pl.canSwitchWeapon()) {
    if(wctrl[WeaponClose]) {
      wctrl[WeaponClose] = !(pl.closeWeapon(false) || pl.isMonster());
      return;
      }
    if(wctrl[WeaponMele]) {
      bool ret=false;
      if(pl.currentMeleWeapon()!=nullptr)
        ret = pl.drawWeaponMele(); else
        ret = pl.drawWeaponFist();
      wctrl[WeaponMele] = !ret;
      wctrlLast         = WeaponMele;
      return;
      }
    if(wctrl[WeaponBow]) {
      if(pl.currentRangeWeapon()!=nullptr){
        wctrl[WeaponBow] = !pl.drawWeaponBow();
        wctrlLast        = WeaponBow;
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
          wctrlLast = static_cast<WeaponAction>(Weapon3+i);
          if(ret) {
            if(auto spl = pl.inventory().currentSpell(i)) {
              Gothic::inst().onPrint(spl->description());
              }
            }
          } else {
          wctrl[Weapon3+i] = false;
          }
        return;
        }
      }
    }

  if(!pl.isInState(ScriptFn()) || dlg.isActive()) {
    runAngleDest = 0;
    return;
    }

  int rotation=0;
  if(allowRot) {
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
    } else {
    rotMouse  = 0;
    rotMouseY = 0;
    }

  pl.setDirectionY(rotY);
  if(pl.isFaling() || pl.isSlide() || pl.isInAir()){
    pl.setDirection(rot);
    runAngleDest = 0;
    return;
    }

  if(casting) {
    if(!actrl[ActForward]) {
      casting = false;
      pl.endCastSpell();
      }
    return;
    }

  if(ctrl[Action::K_ENTER]) {
    pl.transformBack();
    ctrl[Action::K_ENTER] = false;
    }

  if((ws==WeaponState::Bow || ws==WeaponState::CBow) && pl.hasAmunition()) {
    if(actrl[ActGeneric] || actrl[ActForward]) {
      if(auto other = pl.target()) {
        auto dp = other->position()-pl.position();
        pl.turnTo(dp.x,dp.z,false,dt);
        pl.aimBow();
        } else
      if(currentFocus.interactive!=nullptr) {
        auto dp = currentFocus.interactive->position()-pl.position();
        pl.turnTo(dp.x,dp.z,false,dt);
        }
      pl.aimBow();
      if(!actrl[ActForward])
        return;
      }
    }

  if(actrl[ActForward] || actrl[ActMove]) {
    ctrl [Action::Forward] = actrl[ActMove];
    actrl[ActMove]         = false;
    if(ws!=WeaponState::Mage)
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
        if(!casting)
          actrl[ActForward] = false;
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
      if(actrl[ActLeft]) {
        if(pl.swingSwordL())
          ctrl[Action::Left] = false;
        } else
      if(actrl[ActRight]) {
        if(pl.swingSwordR())
          ctrl[Action::Right] = false;
        } else
      if(actrl[ActBack])
        pl.blockSword();

      //ctrl[Action::Back]  = false;

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
      pl.setDirectionY(rotY - rspeed);
      return;
      }
    }
  else if(ctrl[Action::Back]) {
    if((pl.walkMode()&WalkBit::WM_Dive)!=WalkBit::WM_Dive) {
      ani = Npc::Anim::MoveBack;
      } else if(pl.isDive()) {
      pl.setDirectionY(rotY + rspeed);
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
      auto jump = pl.tryJump();
      if(!pl.isFaling() && !pl.isSlide() && jump.anim!=Npc::Anim::Jump){
        pl.startClimb(jump);
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
    processAutoRotate(pl,rot,dt);
    }

  if(ani==Npc::Anim::Move && (rotation!=0 || rotY!=0)) {
    assignRunAngle(pl,rot,dt);
    } else {
    assignRunAngle(pl,pl.rotation(),dt);
    }
  pl.setDirection(rot);
  }

void PlayerControl::implMoveMobsi(Npc& pl, uint64_t /*dt*/) {
  // animation handled in MOBSI
  auto inter = pl.interactive();
  if(ctrl[KeyCodec::Back]) {
    pl.setInteraction(nullptr);
    return;
    }

  if(inter->isStaticState() && !inter->isDetachState(pl)) {
    if(inter->canQuitAtState(pl,inter->stateId())) {
      pl.setInteraction(nullptr,true);
      }
    }
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

void PlayerControl::processAutoRotate(Npc& pl, float& rot, uint64_t dt) {
  if(auto other = pl.target()) {
    if(pl.weaponState()==WeaponState::NoWeapon || other->isDown() || pl.isFinishingMove()){
      pl.setTarget(nullptr);
      }
    else if(!pl.isAtack()) {
      auto  dp   = other->position()-pl.position();
      auto  gl   = pl.guild();
      float step = float(pl.world().script().guildVal().turn_speed[gl]);
      if(actrl[ActGeneric])
        step*=2.f;
      pl.rotateTo(dp.x,dp.z,step,false,dt);
      rot = pl.rotation();
      }
    }
  }
