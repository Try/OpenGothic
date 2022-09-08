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
  auto       w    = Gothic::inst().world();
  auto       pl   = w  ? w->player() : nullptr;
  auto       ws   = pl ? pl->weaponState() : WeaponState::NoWeapon;
  const bool g1c  = Gothic::inst().settingsGetI("GAME","USEGOTHIC1CONTROLS")!=0;
  uint8_t    slot = pl ? pl->inventory().currentSpellSlot() : Item::NSLOT;

  if(pl!=nullptr && pl->interactive()!=nullptr) {
    auto inter = pl->interactive();
    if(inter->needToLockpick(*pl)) {
      processPickLock(*pl,*inter,a);
      return;
      }
    if(inter->isLadder()) {
      processLadder(*pl,*inter,a);
      return;
      }
    }

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

    if(a>=Action::WeaponMage3 && a<=Action::WeaponMage10) {
      int id = (a-Action::WeaponMage3+3);
      if(ws==WeaponState::Mage && slot==id)
        wctrl[WeaponClose] = true; else
        wctrl[id         ] = true;
      return;
      }

    if(key==Tempest::KeyEvent::K_Return)
      ctrl[Action::K_ENTER] = true;
    }

  // this odd behaviour is from original game, seem more like a bug
  const bool actTunneling = (pl!=nullptr && pl->isAtackAnim());

  int fk = -1;
  if(ctrl[KeyCodec::ActionGeneric] || actTunneling || !g1c) {
    if((g1c && a==Action::Forward) || (!g1c && a==Action::ActionGeneric)) {
      if(pl!=nullptr && pl->target()!=nullptr && pl->canFinish(*pl->target()) && !pl->isAtackAnim()) {
        fk = ActKill;
        } else {
        if(!g1c && ctrl[Action::Forward])
          fk = ActMove; else
          fk = ActForward;
        }
      }
    if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H) {
      if(g1c && a==Action::Back)
        fk = ActBack;
      if(!g1c && a==Action::Parade)
        fk = ActBack;
      }
    if(ws!=WeaponState::NoWeapon && g1c && !pl->hasState(BS_RUN)) {
      if(a==Action::Left  || a==Action::RotateL)
        fk = ActLeft;
      if(a==Action::Right || a==Action::RotateR)
        fk = ActRight;
      }
    }


  if(fk>=0) {
    std::memset(actrl,0,sizeof(actrl));
    actrl[ActGeneric] = ctrl[KeyCodec::ActionGeneric];
    actrl[fk]         = true;

    ctrl[a] = true;
    return;
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

  if(a==Action::K_O && Gothic::inst().isMarvinEnabled())
    marvinO();

  ctrl[a] = true;
  }

void PlayerControl::onKeyReleased(KeyCodec::Action a) {
  ctrl[a] = false;

  auto w  = Gothic::inst().world();
  auto pl = w ? w->player() : nullptr;
  if(a==KeyCodec::Map && pl!=nullptr) {
    w->script().playerHotKeyScreenMap(*pl);
    }

  auto ws = pl==nullptr ? WeaponState::NoWeapon : pl->weaponState();
  if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
    if(a==KeyCodec::ActionGeneric)
      std::memset(actrl,0,sizeof(actrl));
    } else {
    std::memset(actrl,0,sizeof(actrl));
    }
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

void PlayerControl::moveFocus(FocusAction act) {
  auto w = Gothic::inst().world();
  auto c = Gothic::inst().camera();
  if(w==nullptr || c==nullptr || currentFocus.npc==nullptr)
    return;

  auto vp  = c->viewProj();
  auto pos = currentFocus.npc->position()+Tempest::Vec3(0,currentFocus.npc->translateY(),0);
  vp.project(pos);

  Npc* next = nullptr;
  auto npos = Tempest::Vec3();
  for(uint32_t i=0; i<w->npcCount(); ++i) {
    auto npc = w->npcById(i);
    if(npc->isPlayer())
      continue;
    auto p = npc->position()+Tempest::Vec3(0,npc->translateY(),0);
    vp.project(p);

    if(std::abs(p.x)>1.f || std::abs(p.y)>1.f || p.z<0.f)
      continue;

    if(!w->testFocusNpc(npc))
      continue;

    if(act==ActLeft && p.x<pos.x && (next==nullptr || npos.x<p.x)) {
      npos = p;
      next = npc;
      }
    if(act==ActRight && p.x>pos.x && (next==nullptr || npos.x>p.x)) {
      npos = p;
      next = npc;
      }
    }

  if(next==nullptr)
    return;
  currentFocus.npc = next;
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

  if(auto c = Gothic::inst().camera())
    c->reset();
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

void PlayerControl::marvinO() {
  auto w = Gothic::inst().world();
  if (w == nullptr || w->player() == nullptr || w->player()->target() == nullptr)
    return;

  auto target = w->player()->target();

  w->setPlayer(target);
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
  if(runAngle!=0.f || std::fabs(runAngleDest)>0.01f) {
    const float speed = 35.f;
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

  Npc::Anim ani = Npc::Anim::Idle;

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
      if(actrl[ActLeft]) {
        moveFocus(ActLeft);
        actrl[ActLeft]  = false;
        }
      if(actrl[ActRight]) {
        moveFocus(ActRight);
        actrl[ActRight]  = false;
        }
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

  if(actrl[ActKill]) {
    if((ws==WeaponState::W1H || ws==WeaponState::W2H) && pl.target()!=nullptr && pl.canFinish(*pl.target()))
      pl.finishingMove();
    actrl[ActKill] = false;
    }

  if(actrl[ActLeft] || actrl[ActRight] || actrl[ActBack]) {
    auto ws = pl.weaponState();
    if(ws==WeaponState::Fist){
      if(actrl[ActBack])
        pl.blockFist();
      return;
      }
    else if(ws==WeaponState::W1H || ws==WeaponState::W2H) {
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
    else if(ws==WeaponState::Mage) {
      if(actrl[ActLeft]) {
        moveFocus(ActLeft);
        actrl[ActLeft]  = false;
        }
      if(actrl[ActRight]) {
        moveFocus(ActRight);
        actrl[ActRight]  = false;
        }
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
    if(pl.bodyStateMasked()==BS_JUMP) {
      ani = Npc::Anim::Idle;
      }
    else if(pl.isDive()) {
      ani = Npc::Anim::Move;
      }
    else if(pl.isSwim()) {
      pl.startDive();
      }
    else if(pl.isInWater()) {
      auto& g  = w->script().guildVal();
      auto  gl = pl.guild();

      if(0<=gl && gl<GIL_MAX) {
        MoveAlgo::JumpStatus jump;
        jump.anim   = Npc::Anim::JumpUp;
        jump.height = float(g.jumpup_height[gl])+pl.position().y;
        pl.startClimb(jump);
        }
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

  if(inter->needToLockpick(pl) && !inter->isCracked()) {
    return;
    }

  if(inter->isStaticState() && !inter->isDetachState(pl)) {
    if(inter->canQuitAtState(pl,inter->stateId())) {
      pl.setInteraction(nullptr,false);
      }
    }
  }

void PlayerControl::processPickLock(Npc& pl, Interactive& inter, KeyCodec::Action k) {
  auto         w             = Gothic::inst().world();
  auto&        script        = w->script();
  const size_t ItKE_lockpick = script.getSymbolIndex("ItKE_lockpick");

  char ch = '\0';
  if(k==KeyCodec::Left || k==KeyCodec::RotateL)
    ch = 'L';
  else if(k==KeyCodec::Right || k==KeyCodec::RotateR)
    ch = 'R';
  else if(k==KeyCodec::Back) {
    quitPicklock(pl);
    return;
    }
  else
    return;

  auto cmp = inter.pickLockCode();
  while(pickLockProgress<cmp.size()) {
    auto c = cmp[pickLockProgress];
    if(c=='l' || c=='L' || c=='r' || c=='R')
      break;
    ++pickLockProgress;
    }

  if(pickLockProgress<cmp.size() && std::toupper(cmp[pickLockProgress])!=ch) {
    pickLockProgress = 0;
    const int32_t dex = pl.attribute(ATR_DEXTERITY);
    if(dex<int32_t(script.rand(100)))  {
      script.invokePickLock(pl,0,1);
      pl.delItem(ItKE_lockpick,1);
      if(pl.inventory().itemCount(ItKE_lockpick)==0) {
        quitPicklock(pl);
        return;
        }
      } else {
      script.invokePickLock(pl,0,0);
      }
    } else {
    pickLockProgress++;
    if(pickLockProgress>=cmp.size()) {
      script.invokePickLock(pl,1,1);
      inter.setAsCracked(true);
      pickLockProgress = 0;
      } else {
      script.invokePickLock(pl,1,0);
      }
    }
  }

void PlayerControl::processLadder(Npc& pl, Interactive& inter, KeyCodec::Action key) {
  if(key==KeyCodec::Back) {
    pl.setInteraction(nullptr);
    return;
    }

  if(key==KeyCodec::Forward) {
    inter.nextState(pl);
    }
  }

void PlayerControl::quitPicklock(Npc& pl) {
  inv.close();
  pickLockProgress = 0;
  pl.setInteraction(nullptr);
  }

void PlayerControl::assignRunAngle(Npc& pl, float rotation, uint64_t dt) {
  float dtF    = (float(dt)/1000.f);
  float angle  = pl.rotation();
  float dangle = (rotation-angle)/dtF;
  auto& wrld   = pl.world();

  if(std::fabs(dangle)<1.f || pl.walkMode()!=WalkBit::WM_Run) {
    if(runAngleSmooth<wrld.tickCount())
      runAngleDest = 0;
    return;
    }

  dangle *= 0.9f;

  float maxV = 15.0f;
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
