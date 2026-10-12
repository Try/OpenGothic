#include "playercontrol.h"

#include <algorithm>
#include <cmath>

#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/objects/interactive.h"
#include "world/world.h"
#include "world/waypath.h"
#include "game/fightalgo.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"
#include "gothic.h"

PlayerControl::PlayerControl(DialogMenu& dlg, InventoryMenu &inv)
  :dlg(dlg),inv(inv) {
  Gothic::inst().onSettingsChanged.bind(this,&PlayerControl::setupSettings);
  setupSettings();
  }

PlayerControl::~PlayerControl() {
  Gothic::inst().onSettingsChanged.ubind(this,&PlayerControl::setupSettings);
  }

void PlayerControl::setArpgCursor(const ArpgCursor& c) {
  arpgCursor = c;
  }

void PlayerControl::setupSettings() {
  if(Gothic::inst().version().game==2) {
    g2Ctrl = Gothic::inst().settingsGetI("GAME","USEGOTHIC1CONTROLS")==0;
    } else {
    g2Ctrl = false;
    }
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

void PlayerControl::onKeyPressed(KeyCodec::Action a, Tempest::KeyEvent::KeyType key, KeyCodec::Mapping mapping) {
  auto       w    = Gothic::inst().world();
  auto       c    = Gothic::inst().camera();
  auto       pl   = w  ? w->player() : nullptr;
  auto       ws   = pl ? pl->weaponState() : WeaponState::NoWeapon;
  uint8_t    slot = pl ? pl->inventory().currentSpellSlot() : Item::NSLOT;

  if(w!=nullptr && w->isCutsceneLock())
    return;

  handleMovementAction(KeyCodec::ActionMapping{a,mapping}, true);

  if(pl!=nullptr && pl->interactive()!=nullptr && c!=nullptr && !c->isFree()) {
    auto inter = pl->interactive();
    if(inter->needToLockpick(*pl)) {
      processPickLock(*pl,*inter,a);
      return;
      }
    if(inter->isLadder()) {
      ctrl[a] = true;
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
        if(wctrlLast==WeaponAction::WeaponBow && pl->currentRangedWeapon()==nullptr)
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
  // const bool actTunneling = (pl!=nullptr && pl->isAttackAnim());
  const bool actTunneling = false;

  int fk = -1;
  if((ctrl[KeyCodec::ActionGeneric] || actTunneling) && !g2Ctrl) {
    if(a==Action::Forward) {
      if(pl!=nullptr && pl->target()!=nullptr && pl->canFinish(*pl->target()) && !pl->isAttackAnim()) {
        fk = ActKill;
        } else {
        fk = ActForward;
        }
      }
    if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H) {
      if(a==Action::Back)
        fk = ActBack;
      }
    if(ws!=WeaponState::NoWeapon && !g2Ctrl && !pl->hasState(BS_RUN)) {
      if(a==Action::Left  || a==Action::RotateL)
        fk = ActLeft;
      if(a==Action::Right || a==Action::RotateR)
        fk = ActRight;
      }
    }

  if(g2Ctrl) {
    if(ws!=WeaponState::NoWeapon) {
      if(a==Action::ActionGeneric) {
        if(pl!=nullptr && pl->target()!=nullptr && pl->canFinish(*pl->target()) && !pl->isAttackAnim()) {
          fk = ActKill;
          } else {
          if(this->wantsToMoveForward())
            fk = ActMove; else
            fk = ActForward;
          }
        }
      }
    if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H) {
      if(a==Action::Parade)
        fk = ActBack;
      }
    if(ws!=WeaponState::NoWeapon && !pl->hasState(BS_RUN)) {
      if(a==Action::ActionLeft)
        fk = ActLeft;
      if(a==Action::ActionRight)
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
    if(this->wantsToMoveForward())
      fk = ActMove;
    std::memset(actrl,0,sizeof(actrl));
    actrl[fk] = true;
    ctrl[a]   = true;
    return;
    }

  if(a==Action::Walk) {
    toggleWalkMode();
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

void PlayerControl::onKeyReleased(KeyCodec::Action a, KeyCodec::Mapping mapping) {
  ctrl[a] = false;

  handleMovementAction(KeyCodec::ActionMapping{a, mapping}, false);

  auto w  = Gothic::inst().world();
  auto pl = w ? w->player() : nullptr;

  if(a==KeyCodec::Map && pl!=nullptr) {
    w->script().playerHotKeyScreenMap(*pl);
    }
  if(a==KeyCodec::Heal && pl!=nullptr) {
    w->script().playerHotLameHeal(*pl);
    }
  if(a==KeyCodec::Potion && pl!=nullptr) {
    w->script().playerHotLamePotion(*pl);
    }

  auto ws = pl==nullptr ? WeaponState::NoWeapon : pl->weaponState();
  if(ws==WeaponState::Bow || ws==WeaponState::CBow || ws==WeaponState::Mage) {
    if(a==KeyCodec::ActionGeneric || (!g2Ctrl && ws==WeaponState::Mage && a==KeyCodec::Forward))
      std::memset(actrl,0,sizeof(actrl));
    } else {
    std::memset(actrl,0,sizeof(actrl));
    }
  }

auto PlayerControl::handleMovementAction(KeyCodec::ActionMapping actionMapping, bool pressed) -> void {
  auto[action, mapping] = actionMapping;
  auto mappingIndex = (mapping == KeyCodec::Mapping::Primary ? size_t(0) : size_t(1));
  if (action == Action::Forward)
    movement.forwardBackward.main[mappingIndex] = pressed;
  else if (action == Action::Back)
    movement.forwardBackward.reverse[mappingIndex] = pressed;
  else if (action == Action::Right)
    movement.strafeRightLeft.main[mappingIndex] = pressed;
  else if (action == Action::Left)
    movement.strafeRightLeft.reverse[mappingIndex] = pressed;
  else if (action == Action::RotateR)
    movement.turnRightLeft.main[mappingIndex] = pressed;
  else if (action == Action::RotateL)
    movement.turnRightLeft.reverse[mappingIndex] = pressed;
  }

bool PlayerControl::isPressed(KeyCodec::Action a) const {
  return ctrl[a];
  }

void PlayerControl::onRotateMouse(float dAngleX, float dAngleY) {
  rotMouse  += dAngleX;
  rotMouseY += dAngleY;
  }

void PlayerControl::drawVobRay(DbgPainter& p) const {
  auto w = Gothic::inst().world();
  if(w==nullptr || w->player()==nullptr)
    return;
  auto pl    = w->player();
  auto focus = findFocus(&currentFocus);
  if(focus.interactive!=nullptr) {
    focus.interactive->drawVobRay(p, *pl);
    }
  if(focus.item!=nullptr) {
    focus.item->drawVobRay(p, *pl);
    }
  if(focus.npc!=nullptr) {
    pl->drawVobRay(p, *focus.npc);
    }
  }

void PlayerControl::tickFocus() {
  auto camera = Gothic::inst().camera();
  if(camera!=nullptr && camera->isArpgActive() && arpgCursor.valid) {
    // gothic-diablo: focus is whatever is under the mouse cursor
    currentFocus = arpgPick();
    arpgHover    = currentFocus;
    if(arpgClick.kind==ArpgClick::Attack && arpgClick.target.npc!=nullptr)
      setTarget(arpgClick.target.npc); else
      setTarget(currentFocus.npc);
    return;
    }
  arpgHover       = Focus();
  arpgGroundValid = false;

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
  auto state = pl->bodyStateMasked();
  if(other.isDown()) {
    if(state!=BS_STAND && state!=BS_SNEAK && state!=BS_SWIM && state!=BS_DIVE)
      return false;
    if(!inv.ransack(*w->player(),other))
      w->script().printNothingToGet();
    } else {
    if((state&BS_MAX)!=BS_NONE)
      return false;
    other.startDialog(*pl);
    }
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
  auto pos = currentFocus.npc->centerPosition();
  vp.project(pos);

  Npc* next = nullptr;
  auto npos = Tempest::Vec3();
  for(uint32_t i=0; i<w->npcCount(); ++i) {
    auto npc = w->npcById(i);
    if(npc->isPlayer())
      continue;
    auto p = npc->centerPosition();
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

void PlayerControl::toggleWalkMode() {
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
  if(!pl->canSneak() || (pl->walkMode()&WalkBit::WM_Sneak)==WalkBit::WM_Sneak) {
    pl->setWalkMode(pl->walkMode() & (~WalkBit::WM_Sneak));
    } else {
    pl->setWalkMode(pl->walkMode()^WalkBit::WM_Sneak);
    }
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
  movement.reset();
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

  Tempest::Vec3 dp(c,0.8f,s);
  pos += dp*6000*float(dt)/1000.f;

  pl.changeAttribute(ATR_HITPOINTS,pl.attribute(ATR_HITPOINTSMAX),false);
  pl.changeAttribute(ATR_MANA,     pl.attribute(ATR_MANAMAX),     false);
  pl.clearState(false);
  pl.clearSpeed();
  pl.clearAiQueue();
  pl.setPosition(pos);
  pl.setInteraction(nullptr,true);
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

  Tempest::Vec3 dp(c, 0.0f, s);
  pos += dp * 6000 * float(dt) / 1000.f;

  pl.clearState(false);
  pl.clearSpeed();
  pl.setPosition(pos);
  pl.setInteraction(nullptr,true);
  // pl.setAnim(AnimationSolver::Idle); // Original G2 behaviour: K doesn't stop running
  }

void PlayerControl::marvinO() {
  auto w = Gothic::inst().world();
  if (w == nullptr || w->player() == nullptr || w->player()->target() == nullptr)
    return;

  auto target = w->player()->target();

  w->setPlayer(target);
  }

Focus PlayerControl::findFocus(const Focus* prev) const {
  auto w = Gothic::inst().world();
  auto c = Gothic::inst().camera();
  if(w==nullptr)
    return Focus();
  if(w->player()!=nullptr && w->player()->isDown())
    return Focus();
  if(c!=nullptr && c->isCutscene())
    return Focus();
  if(!cacheFocus)
    prev = nullptr;

  if(prev)
    return w->findFocus(*prev);
  return w->findFocus(Focus());
  }

bool PlayerControl::tickCameraMove(uint64_t dt) {
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return false;

  Npc*  pl     = w->player();
  auto  camera = Gothic::inst().camera();
  if(camera==nullptr || (pl!=nullptr && !camera->isFree()))
    return false;

  rotMouse = 0;
  if(ctrl[KeyCodec::Left] || (ctrl[KeyCodec::RotateL] && ctrl[KeyCodec::Jump])) {
    camera->moveLeft(dt);
    return true;
    }
  if(ctrl[KeyCodec::Right] || (ctrl[KeyCodec::RotateR] && ctrl[KeyCodec::Jump])) {
    camera->moveRight(dt);
    return true;
    }

  auto turningVal = movement.turnRightLeft.value();
  if(turningVal > 0.f)
    camera->rotateRight(dt);
  else if(turningVal < 0.f)
    camera->rotateLeft(dt);

  auto forwardVal = movement.forwardBackward.value();
  if(forwardVal > 0.f)
    camera->moveForward(dt);
  else if(forwardVal < 0.f)
    camera->moveBack(dt);
  return true;
  }

bool PlayerControl::tickMove(uint64_t dt) {
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return false;
  const float dtF = float(dt)/1000.f;

  Npc*  pl     = w->player();
  auto  camera = Gothic::inst().camera();

  if(w->isCutsceneLock())
    clearInput();

  if(tickCameraMove(dt))
    return true;

  if(ctrl[Action::K_F8] && Gothic::inst().isMarvinEnabled())
    marvinF8(dt);
  if(ctrl[Action::K_K] && Gothic::inst().isMarvinEnabled())
    marvinK(dt);
  cacheFocus = ctrl[Action::ActionGeneric];
  if(camera!=nullptr)
    camera->setLookBack(ctrl[Action::LookBack]);

  if(pl==nullptr)
    return true;

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
  auto  w         = Gothic::inst().world();
  Npc&  pl        = *w->player();
  float rot       = pl.rotation();
  float rotY      = pl.rotationY();
  // 100 / 200 according to some sources, yet my mesures are 90/180
  float rspeed    = (pl.weaponState()==WeaponState::NoWeapon ? 90.f : 180.f)*(float(dt)/1000.f);
  auto  ws        = pl.weaponState();
  auto  bs        = pl.bodyStateMasked();
  bool  allowRot  = !ctrl[KeyCodec::ActionGeneric] && pl.isRotationAllowed();
  // gothic-diablo: isometric ARPG controls (screen-relative movement)
  auto  camera    = Gothic::inst().camera();
  bool  arpg      = (camera!=nullptr && camera->isArpgActive());

  Npc::Anim ani = Npc::Anim::Idle;

  if(bs==BS_DEAD)
    return;
  if(bs==BS_UNCONSCIOUS)
    return;

  if(!pl.isAiQueueEmpty()) {
    runAngleDest = 0;
    return;
    }

  if(pl.interactive()!=nullptr) {
    runAngleDest = 0;
    implMoveMobsi(pl,dt);
    return;
    }

  if(pl.canSwitchWeapon()) {
    if(wctrl[WeaponClose]) {
      wctrl[WeaponClose] = !(pl.closeWeapon(false) || pl.isMonster());
      return;
      }
    if(wctrl[WeaponMele]) {
      bool ret=false;
      if(pl.currentMeleeWeapon()!=nullptr)
        ret = pl.drawWeaponMelee(); else
        ret = pl.drawWeaponFist();
      wctrl[WeaponMele] = !ret;
      wctrlLast         = WeaponMele;
      if(!wctrl[WeaponMele])
        return;
      }
    if(wctrl[WeaponBow]) {
      if(pl.currentRangedWeapon()!=nullptr) {
        wctrl[WeaponBow] = !pl.drawWeaponBow();
        wctrlLast        = WeaponBow;
        } else {
        wctrl[WeaponBow] = false;
        }
      if(!wctrl[WeaponBow])
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
          return;
          }
        }
      }
    }

  if(!pl.isInState(ScriptFn()) || dlg.isActive()) {
    runAngleDest = 0;
    return;
    }

  int rotation = 0;
  if(allowRot && arpg) {
    // turn keys rotate the ARPG camera; mouse does not rotate the hero
    const float camSpeed = 90.f*(float(dt)/1000.f);
    if(this->wantsToTurnLeft())
      camera->arpgRotate(+camSpeed);
    if(this->wantsToTurnRight())
      camera->arpgRotate(-camSpeed);
    rotMouse  = 0;
    rotMouseY = 0;
    }
  else if(allowRot) {
    if(this->wantsToTurnLeft()) {
      rot += rspeed;
      rotation = -1;
      rotMouse=0;
      }
    if(this->wantsToTurnRight()) {
      rot -= rspeed;
      rotation = 1;
      rotMouse=0;
      }
    if(std::fabs(rotMouse)>0.f) {
      if(rotMouse>0)
        rotation = -1; else
        rotation = 1;
      rot += rotMouse;
      rotMouse  = 0;
      }
    rotY+=rotMouseY;
    } else {
    rotMouse  = 0;
    rotMouseY = 0;
    }

  pl.setDirectionY(rotY);
  if(pl.isFalling() || pl.isSlide() || pl.isInAir() || pl.isJump() || pl.isJumpUp()){
    pl.setDirection(rot);
    runAngleDest = 0;
    return;
    }

  if(casting) {
    if(!actrl[ActForward] || (Gothic::inst().version().game==1 && pl.attribute(ATR_MANA)==0)) {
      casting = false;
      pl.endCastSpell(true);
      }
    return;
    }

  if(ctrl[Action::K_ENTER]) {
    pl.transformBack();
    ctrl[Action::K_ENTER] = false;
    }

  if((ws==WeaponState::Bow || ws==WeaponState::CBow) && pl.hasAmmunition()) {
    if(actrl[ActGeneric] || actrl[ActForward]) {
      if(auto other = pl.target()) {
        auto dp = other->position()-pl.position();
        pl.turnTo(dp.x,dp.z,true,dt);
        pl.aimBow();
        }
      else if(currentFocus.interactive!=nullptr) {
        auto dp = currentFocus.interactive->position()-pl.position();
        pl.turnTo(dp.x,dp.z,false,dt);
        pl.aimBow();
        }
      else {
        pl.aimBow();
        }

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

  if(ws==WeaponState::Mage) {
    if(actrl[ActGeneric] || actrl[ActForward] || ctrl[KeyCodec::ActionGeneric]) {
      if(auto other = pl.target()) {
        auto dp = other->centerPosition() - pl.centerPosition();
        pl.turnTo(dp.x,dp.z,true,dt);
        } else
      if(currentFocus.interactive!=nullptr) {
        auto dp = currentFocus.interactive->position()-pl.position();
        pl.turnTo(dp.x,dp.z,false,dt);
        }

      if(actrl[ActLeft]) {
        moveFocus(ActLeft);
        actrl[ActLeft]  = false;
        }
      if(actrl[ActRight]) {
        moveFocus(ActRight);
        actrl[ActRight]  = false;
        }
      if(!actrl[ActForward]) {
        pl.setAnim(Npc::Anim::Idle);
        return;
        }
      }
    }

  if(actrl[ActForward] || actrl[ActMove]) {
    ctrl [Action::Forward] = actrl[ActMove];
    actrl[ActMove]         = false;
    if(ws!=WeaponState::Mage && !(g2Ctrl && (ws==WeaponState::Bow || ws==WeaponState::CBow))) {
      actrl[ActForward] = false;
      if(!ctrl[Action::Forward])
        movement.reset();
      }
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
        casting = (pl.beginCastSpell()==Npc::BC_Invest);
        if(!casting)
          actrl[ActForward] = false;
        return;
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
    if(ws==WeaponState::Fist) {
      if(actrl[ActBack])
        pl.blockFist();
      return;
      }
    else if(ws==WeaponState::W1H || ws==WeaponState::W2H) {
      if(actrl[ActLeft] && pl.swingSwordL()) {
        movement.strafeRightLeft.reset();
        }
      else if(actrl[ActRight] && pl.swingSwordR()) {
        movement.strafeRightLeft.reset();
        }
      else if(actrl[ActBack] && pl.blockSword()) {
        // movement.forwardBackward.reset();
        }

      actrl[ActLeft]  = false;
      actrl[ActRight] = false;
      // actrl[ActBack]  = false;
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

  const bool arpgMove = arpg && (!ctrl[KeyCodec::ActionGeneric] || arpgClick.kind!=ArpgClick::None) &&
                        (pl.walkMode()&WalkBit::WM_Dive)!=WalkBit::WM_Dive;
  if(arpgMove) {
    // W/S/A/D (and arrows) move up/down/left/right on screen, hero turns to the move direction
    const float fb = movement.forwardBackward.value();
    const float lr = movement.strafeRightLeft.value();
    if(fb!=0.f || lr!=0.f)
      arpgClick = ArpgClick(); // keyboard overrides mouse
    if(fb==0.f && lr==0.f && arpgClick.kind!=ArpgClick::None) {
      // left mouse button: walk / interact / attack
      switch(arpgClickTick(pl, rot, rotation, pl.isRotationAllowed(), dt)) {
        case ArpgStop: ani = Npc::Anim::Idle; rotation = 0; break;
        case ArpgTurn: ani = Npc::Anim::Idle; break;
        case ArpgMove: ani = Npc::Anim::Move; break;
        }
      }
    else if(fb!=0.f || lr!=0.f) {
      const float dest = camera->arpgYaw() + std::atan2(-lr, fb)*180.f/float(M_PI);
      const float da   = std::remainder(dest-rot, 360.f);
      if(allowRot) {
        const float step = 900.f*(float(dt)/1000.f);
        rot += std::clamp(da, -step, step);
        }
      if(std::fabs(da)<100.f || !allowRot) {
        ani = Npc::Anim::Move;
        } else {
        ani      = Npc::Anim::Idle;
        rotation = (da>0.f) ? -1 : 1;
        }
      }
    }
  else if(this->wantsToStrafeLeft()) {
    ani = Npc::Anim::MoveL;
    }
  else if(this->wantsToStrafeRight()) {
    ani = Npc::Anim::MoveR;
    }
  else if(this->wantsToMoveForward()) {
    if((pl.walkMode()&WalkBit::WM_Dive)!=WalkBit::WM_Dive) {
      ani = Npc::Anim::Move;
      }
    else if(pl.isDive()) {
      pl.setDirectionY(rotY - rspeed);
      return;
      }
    }
  else if(this->wantsToMoveBackward()) {
    if((pl.walkMode()&WalkBit::WM_Dive)!=WalkBit::WM_Dive) {
      ani = Npc::Anim::MoveBack;
      } else if(pl.isDive()) {
      pl.setDirectionY(rotY + rspeed);
      return;
      }
    }


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

      if(0<=gl && gl<GIL_MAX && pl.isStanding()) {
        MoveAlgo::JumpStatus jump;
        jump.anim   = Npc::Anim::JumpUp;
        jump.height = float(g.jumpup_height[gl])+pl.position().y;
        pl.startClimb(jump);
        }
      }
    else if(pl.isStanding()) {
      auto jump = pl.tryJump();
      if(!pl.isFalling() && !pl.isSlide() && jump.anim!=Npc::Anim::Jump){
        pl.startClimb(jump);
        return;
        }
      ani = Npc::Anim::Jump;
      }
    else if(!pl.isAttackAnim() && !pl.isCasting()) {
      ani = Npc::Anim::Jump;
      }
    }

  if(!pl.isCasting()) {
    if(ani==Npc::Anim::Jump) {
      pl.setAnimRotate(0);
      rotation = 0;
      }

    if(pl.isAttackAnim()) {
      if((ani==Npc::Anim::MoveL || ani==Npc::Anim::MoveR/* || ani==Npc::Anim::MoveBack*/) && pl.hasState(BS_RUN)) {
        ani = Npc::Anim::Idle;
        }

      if(!pl.hasState(BS_RUN) && ani==Npc::Anim::Idle) {
        // charge-run
        ani = Npc::Anim::NoAnim;
        }
      if((ani==Npc::Anim::MoveL || ani==Npc::Anim::MoveR) &&
          pl.hasState(BS_STAND) && pl.hasState(BS_HIT)) {
        // no charge to strafe transition
        ani = Npc::Anim::NoAnim;
        }
      }

    if(bs==BS_LIE) {
      ani = (ani==Npc::Anim::Move) ? Npc::Anim::Idle : Npc::Anim::NoAnim;
      rot = pl.rotation();
      }

    if(ani!=Npc::Anim::NoAnim)
      pl.setAnim(ani);
    }

  setAnimRotate(pl, rot, ani==Npc::Anim::Idle ? rotation : 0, movement.turnRightLeft.any(), dt);
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

  if(ctrl[KeyCodec::Back] && !inter->isLadder()) {
    pl.setInteraction(nullptr);
    return;
    }

  if(inter->needToLockpick(pl) && !inter->isCracked()) {
    return;
    }

  if(!inter->isLadder() && inter->isStaticState() && !inter->isDetachState(pl)) {
    auto stateId = inter->stateId();
    if(inter->canQuitAtState(pl,stateId))
      pl.setInteraction(nullptr,false);
    }

  if(inter->isLadder()) {
    if(ctrl[KeyCodec::ActionGeneric]) {
      inter->onKeyInput(KeyCodec::ActionGeneric);
      ctrl[KeyCodec::ActionGeneric] = false;
      }
    else if(ctrl[KeyCodec::Forward]) {
      inter->onKeyInput(KeyCodec::Forward);
      }
    else if(ctrl[KeyCodec::Back]) {
      inter->onKeyInput(KeyCodec::Back);
      }
    }
  }

void PlayerControl::processPickLock(Npc& pl, Interactive& inter, KeyCodec::Action k) {
  auto                   w             = Gothic::inst().world();
  auto&                  script        = w->script();
  const size_t           ItKE_lockpick = script.lockPickId();

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
    const int32_t dex = Gothic::inst().version().game==2 ? pl.attribute(ATR_DEXTERITY) : (100 - pl.talentValue(TALENT_PICKLOCK));
    if(dex<=int32_t(script.rand(100)))  {
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
  if(key!=KeyCodec::ActionGeneric && key!=KeyCodec::Forward && key!=KeyCodec::Back)
    return;

  ctrl[key] = true;
  inter.onKeyInput(key);
  }

void PlayerControl::quitPicklock(Npc& pl) {
  inv.close();
  pickLockProgress = 0;
  pl.setInteraction(nullptr);
  }

void PlayerControl::assignRunAngle(Npc& pl, float rotation, uint64_t dt) {
  float dtF    = (float(dt)/1000.f);
  auto  camera = Gothic::inst().camera();

  float dest = 0;
  if(camera!=nullptr && pl.walkMode()==WalkBit::WM_Run && pl.bodyState()==BS_RUN) {
    const float az   = camera->azimuth();
    const float maxV = 14.5f;
    dest = std::min(std::abs(az), maxV)*(az>=0 ? 1 : -1);
    }

  float a = std::min(dtF*5.f, 1.f);
  runAngleDest = runAngleDest*(1.f-a)+dest*a;
  }

void PlayerControl::setAnimRotate(Npc& pl, float rotation, int anim, bool force, uint64_t dt) {
  float dtF    = (float(dt)/1000.f);
  float angle  = pl.rotation();
  float dangle = (rotation-angle)/dtF;
  auto& wrld   = pl.world();

  if(std::fabs(dangle)<30.f && !force) // 30 deg per second threshold
    anim = 0;
  if(anim!=0 && pl.isAttackAnim())
    anim = 0;
  if(rotationAni==anim && anim!=0)
    force = true;
  if(!force && wrld.tickCount()<turnAniSmooth)
    return;
  turnAniSmooth = wrld.tickCount() + 100;
  rotationAni   = anim;
  pl.setAnimRotate(anim);
  }

void PlayerControl::processAutoRotate(Npc& pl, float& rot, uint64_t dt) {
  if(auto other = pl.target()) {
    if(pl.weaponState()==WeaponState::NoWeapon || pl.isFinishingMove()){
      pl.setTarget(nullptr);
      }
    else if(!pl.isAttack()) {
      auto  dp   = other->centerPosition() - pl.centerPosition();
      auto  gl   = pl.guild();
      float step = float(pl.world().script().guildVal().turn_speed[gl]);
      if(actrl[ActGeneric])
        step*=2.f;
      pl.rotateTo(dp.x,dp.z,step,AnimationSolver::TurnType::Std,dt);
      rot = pl.rotation();
      }
    }
  }

// gothic-diablo: Diablo-like mouse controls ////////////////////////////////////////////

static bool arpgIsHostile(const Npc& pl, const Npc& npc) {
  if(npc.isMonster())
    return true;
  if(Gothic::inst().version().game==2 && npc.guild()>GIL_SEPERATOR_ORC)
    return true; // orcs and other non-human creatures
  return npc.isEnemy(pl);
  }

Focus PlayerControl::arpgPick() {
  arpgGroundValid = false;

  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  if(w==nullptr || pl==nullptr || !arpgCursor.valid)
    return Focus();

  auto&         physic = *w->physic();
  Tempest::Vec3 from   = arpgCursor.rayFrom;
  Tempest::Vec3 dir    = arpgCursor.rayTo - from;
  const float   len    = dir.length();
  if(len<1.f)
    return Focus();
  dir = dir/len;
  const Tempest::Vec3 to = from + dir*std::min(len, 8000.f);

  const auto land = physic.ray(from, to);
  if(land.hasCol) {
    arpgGround      = land.v;
    arpgGroundValid = true;
    }

  // 1. creature under the cursor (collision capsule)
  {
    auto hit = physic.rayNpc(from, to, pl);
    if(hit.npcHit!=nullptr && hit.npcHit!=pl)
      return Focus(*hit.npcHit);
  }
  if(!arpgGroundValid)
    return Focus();

  const Tempest::Vec3 g     = arpgGround;
  const float         scale = std::max(arpgCursor.screen.y, 1.f)/1080.f;
  auto screenDist = [this](const Tempest::Vec3& p) {
    Tempest::Vec4 v = {p.x, p.y, p.z, 1.f};
    arpgCursor.viewProj.project(v);
    if(v.w<=0.f)
      return 1e9f;
    const float sx = (v.x/v.w*0.5f+0.5f)*arpgCursor.screen.x;
    const float sy = (v.y/v.w*0.5f+0.5f)*arpgCursor.screen.y;
    return std::hypot(sx-arpgCursor.pixel.x, sy-arpgCursor.pixel.y);
    };

  // 2. item lying near the cursor
  {
    Item* best  = nullptr;
    float bestD = 40.f*scale;
    w->forEachItemNear(g, 300.f, [&](Item& it) {
      const float d = screenDist(it.midPosition());
      if(d<bestD) {
        bestD = d;
        best  = &it;
        }
      });
    if(best!=nullptr)
      return Focus(*best);
  }

  // 3. usable object: hit by the ray, or near the cursor
  if(land.vob!=nullptr && !land.vob->focusName().empty())
    return Focus(*land.vob);
  {
    Interactive* best  = nullptr;
    float        bestD = 45.f*scale;
    w->forEachInteractiveNear(g, 300.f, [&](Interactive& m) {
      if(m.focusName().empty())
        return;
      const float d = screenDist(m.displayPosition());
      if(d<bestD) {
        bestD = d;
        best  = &m;
        }
      });
    if(best!=nullptr)
      return Focus(*best);
  }

  // 4. creature near the cursor (small or fast ones are hard to hit with a ray)
  {
    Npc*  best  = nullptr;
    float bestD = 50.f*scale;
    for(uint32_t i=0; i<w->npcCount(); ++i) {
      auto npc = w->npcById(i);
      if(npc==nullptr || npc==pl)
        continue;
      if((npc->position()-g).quadLength() > 600.f*600.f)
        continue;
      const float d = screenDist(npc->centerPosition());
      if(d<bestD) {
        bestD = d;
        best  = npc;
        }
      }
    if(best!=nullptr)
      return Focus(*best);
  }
  return Focus();
  }

void PlayerControl::arpgMouseDown() {
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  arpgClick      = ArpgClick();
  arpgClick.held = true;
  if(pl==nullptr)
    return;

  const bool force = ctrl[KeyCodec::ActionGeneric]; // Ctrl + click: attack anyone
  const auto f     = arpgHover;
  if(f.npc!=nullptr) {
    arpgClick.target = Focus(*f.npc);
    if(f.npc->isDown())
      arpgClick.kind = ArpgClick::Loot;
    else if(force || arpgIsHostile(*pl,*f.npc))
      arpgClick.kind = ArpgClick::Attack;
    else
      arpgClick.kind = ArpgClick::Talk;
    }
  else if(f.item!=nullptr) {
    arpgClick.kind   = ArpgClick::PickUp;
    arpgClick.target = Focus(*f.item);
    }
  else if(f.interactive!=nullptr) {
    arpgClick.kind   = ArpgClick::Use;
    arpgClick.target = Focus(*f.interactive);
    }
  else if(arpgGroundValid) {
    arpgClick.kind  = ArpgClick::Ground;
    arpgClick.point = arpgGround;
    }

  if(arpgClick.kind==ArpgClick::Attack && pl->weaponState()==WeaponState::NoWeapon)
    wctrl[WeaponMele] = true; // draw the weapon while running to the enemy
  arpgClick.stuckPos = pl->position();
  }

void PlayerControl::arpgMouseUp() {
  arpgClick.held = false;
  }

bool PlayerControl::arpgLineClear(const Tempest::Vec3& from, const Tempest::Vec3& to) const {
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return true;
  const Tempest::Vec3 up = {0, 90.f, 0};
  return !w->physic()->ray(from+up, to+up).hasCol;
  }

Tempest::Vec3 PlayerControl::arpgNextPoint(Npc& pl, const Tempest::Vec3& dest, uint64_t dt) {
  auto&       c   = arpgClick;
  auto        w   = Gothic::inst().world();
  const auto  pos = pl.position();
  const float dtF = float(dt)/1000.f;

  // re-plan twice a second: straight line if nothing is in the way, otherwise the waynet
  c.repathTimer -= dtF;
  if(c.repathTimer<=0.f) {
    c.repathTimer = 0.5f;
    c.path.clear();
    c.pathIdx = 0;
    if(c.useWaynet || !arpgLineClear(pos,dest)) {
      if(auto end = w->findWayPoint(dest)) {
        WayPath way = w->wayTo(pl,*end);
        while(auto wp = way.pop())
          c.path.push_back(wp->position());
        }
      }
    }

  while(c.pathIdx<c.path.size()) {
    const auto d = c.path[c.pathIdx]-pos;
    if(d.x*d.x+d.z*d.z > 90.f*90.f)
      break;
    ++c.pathIdx;
    }

  // stuck: first try the waynet, then give up
  c.stuckTimer += dtF;
  if(c.stuckTimer>=1.f) {
    const auto  d     = pos-c.stuckPos;
    const float moved = d.x*d.x+d.z*d.z;
    c.stuckTimer = 0;
    c.stuckPos   = pos;
    if(moved<25.f*25.f) {
      if(!c.useWaynet) {
        c.useWaynet   = true;
        c.repathTimer = 0;
        } else {
        c.kind = ArpgClick::None;
        }
      }
    }

  if(c.pathIdx<c.path.size())
    return c.path[c.pathIdx];
  return dest;
  }

PlayerControl::ArpgStep PlayerControl::arpgClickTick(Npc& pl, float& rot, int& rotation, bool allowRot, uint64_t dt) {
  auto  w = Gothic::inst().world();
  auto& c = arpgClick;
  if(w==nullptr || c.kind==ArpgClick::None)
    return ArpgStop;
  if(dlg.isActive() || inv.isActive()) {
    c = ArpgClick();
    return ArpgStop;
    }

  if(c.kind!=ArpgClick::Ground) {
    c.target = w->validateFocus(c.target);
    if(c.target.npc==nullptr && c.target.item==nullptr && c.target.interactive==nullptr) {
      c = ArpgClick();
      return ArpgStop;
      }
    }

  // where to go and how close is close enough
  Tempest::Vec3 dest  = c.point;
  float         reach = 50.f;
  switch(c.kind) {
    case ArpgClick::None:
      return ArpgStop;
    case ArpgClick::Ground:
      if(c.held && arpgGroundValid)
        c.point = arpgGround; // holding the button: follow the cursor
      dest  = c.point;
      reach = 50.f;
      break;
    case ArpgClick::PickUp:
      dest  = c.target.item->position();
      reach = 110.f;
      break;
    case ArpgClick::Use:
      dest  = c.target.interactive->nearestPoint(pl);
      reach = 130.f;
      break;
    case ArpgClick::Talk:
      dest  = c.target.npc->position();
      reach = 250.f;
      break;
    case ArpgClick::Loot:
      dest  = c.target.npc->position();
      reach = 150.f;
      break;
    case ArpgClick::Attack:
      dest  = c.target.npc->position();
      reach = 150.f;
      break;
    }

  const auto pos    = pl.position();
  const auto d      = dest-pos;
  const float dist2 = d.x*d.x+d.z*d.z;
  const float step  = 900.f*(float(dt)/1000.f);

  bool inRange = dist2<=reach*reach;
  if(c.kind==ArpgClick::Attack) {
    const auto ws = pl.weaponState();
    if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H) {
      FightAlgo fa;
      inRange = fa.isInAttackRange(pl, *c.target.npc, w->script());
      }
    }

  if(!inRange) {
    const auto  next = arpgNextPoint(pl, dest, dt);
    if(c.kind==ArpgClick::None)
      return ArpgStop; // gave up: stuck
    const float ang  = std::atan2(next.z-pos.z, next.x-pos.x)*180.f/float(M_PI);
    const float da   = std::remainder(ang-rot, 360.f);
    if(allowRot)
      rot += std::clamp(da, -step, step);
    if(std::fabs(da)<100.f)
      return ArpgMove;
    rotation = (da>0.f) ? -1 : 1;
    return ArpgTurn;
    }

  // in range: face the target
  float faceDa = 0;
  if(c.kind!=ArpgClick::Ground && dist2>1.f) {
    const float ang = std::atan2(d.z, d.x)*180.f/float(M_PI);
    faceDa = std::remainder(ang-rot, 360.f);
    if(allowRot)
      rot += std::clamp(faceDa, -step, step);
    }

  switch(c.kind) {
    case ArpgClick::None:
      break;
    case ArpgClick::Ground:
      if(c.held)
        return ArpgStop; // button still down: wait for the cursor to move
      break;
    case ArpgClick::PickUp:
      interact(*c.target.item);
      break;
    case ArpgClick::Use:
      interact(*c.target.interactive);
      break;
    case ArpgClick::Loot:
      interact(*c.target.npc);
      break;
    case ArpgClick::Talk:
      if(pl.weaponState()!=WeaponState::NoWeapon) {
        wctrl[WeaponClose] = true; // put the weapon away, then talk
        return ArpgStop;
        }
      interact(*c.target.npc);
      break;
    case ArpgClick::Attack: {
      auto& tg = *c.target.npc;
      if(tg.isDown() || tg.isDead())
        break;
      const auto ws = pl.weaponState();
      if(ws==WeaponState::NoWeapon) {
        wctrl[WeaponMele] = true;
        return ArpgStop;
        }
      if(ws!=WeaponState::Fist && ws!=WeaponState::W1H && ws!=WeaponState::W2H)
        break; // bows and spells: next phase
      if(std::fabs(faceDa)<30.f && !pl.isAttackAnim()) {
        if(!c.held && c.swings>0)
          break; // single click: one attack
        if(ws==WeaponState::Fist)
          pl.fistShoot(); else
          pl.swingSword();
        c.swings++;
        }
      return ArpgStop;
      }
    }
  c = ArpgClick();
  return ArpgStop;
  }
