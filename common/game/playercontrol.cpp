#include "playercontrol.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/objects/interactive.h"
#include "world/world.h"
#include "world/waypath.h"
#include "game/fightalgo.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"
#include "gothic.h"
#include "game/gamesession.h"
#include "game/arpgskills.h"
#include "game/arpgkeys.h"
#include "ui/arpgmenu.h"
#include "physics/dynamicworld.h"

#include <Tempest/Log>

// gothic-diablo: what the hero's skill trees give (defaults without a game)
static const ArpgPerks& heroPerks() {
  static const ArpgPerks none;
  if(auto gs = Gothic::inst().gameSession())
    return gs->arpgProgress().perks();
  return none;
  }

float PlayerControl::arpgChargeTimeFull() const {
  return heroPerks().chargeTime;
  }

float PlayerControl::arpgChargeTimeWindow() const {
  return heroPerks().chargeWindow;
  }

float PlayerControl::arpgSpecialCooldown() const {
  return std::max(0.5f, heroPerks().chargeCd);
  }

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

  if(c!=nullptr && c->isArpgActive()) {
    const int ak = ArpgKeys::actionOf(key);
    if(ak>=0)
      arpgHeld[ak] = true;
    }

  // Diathic: a key of the ARPG mode does not move the hero as well (Gothic's binding of the same key)
  if(!(pl!=nullptr && c!=nullptr && c->isArpgActive() && ArpgKeys::actionOf(key)>=0))
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

  const bool arpgKeys = (pl!=nullptr && c!=nullptr && c->isArpgActive());
  if(arpgKeys) {
    // Diathic: the keys of the ARPG mode (ArpgKeys, "Sterowanie" in the main menu) win over Gothic's
    const int ak = ArpgKeys::actionOf(key);
    if(ak==ArpgKeys::Dodge) {
      arpgDodge();
      return;
      }
    if(ak==ArpgKeys::SwapWeapon) {
      arpgSwapWeapon();
      return;
      }
    if(ak==ArpgKeys::SkillA || ak==ArpgKeys::SkillB || ak==ArpgKeys::SkillBlock) {
      arpgSkillKey(ak==ArpgKeys::SkillA ? 0 : (ak==ArpgKeys::SkillB ? 1 : 2)); // Diathic: the weapon trees
      return;
      }
    if(ak==ArpgKeys::Sneak) {
      // Gothic: only with the talent (learned from a teacher)
      if(!pl->canSneak() && (pl->walkMode()&WalkBit::WM_Sneak)!=WalkBit::WM_Sneak) {
        Gothic::inst().onPrint(ArpgMenu::gtxt("Nie umiesz się skradać - naucz się tego u nauczyciela"));
        return;
        }
      toggleSneakMode();
      const bool on = (pl->walkMode()&WalkBit::WM_Sneak)==WalkBit::WM_Sneak;
      Gothic::inst().onPrint(ArpgMenu::gtxt(on ? "Skradanie" : "Koniec skradania"));
      return;
      }
    if(ak==ArpgKeys::Walk) {
      toggleWalkMode();
      const bool walk = (pl->walkMode()&WalkBit::WM_Walk)==WalkBit::WM_Walk;
      Gothic::inst().onPrint(ArpgMenu::gtxt(walk ? "Chód" : "Bieg"));
      return;
      }
    if(ak==ArpgKeys::PotionHealth || ak==ArpgKeys::PotionMana) {
      arpgPotion(ak==ArpgKeys::PotionHealth ? 1 : 2);
      return;
      }
    if(ak==ArpgKeys::Weapon) {
      // draw / put away the weapon
      if(ws!=WeaponState::NoWeapon) {
        wctrl[WeaponClose] = true;
        } else {
        if(wctrlLast>=WeaponAction::Weapon3 && pl->inventory().currentSpell(static_cast<uint8_t>(wctrlLast-3))==nullptr)
          wctrlLast = WeaponAction::WeaponBow;
        if(wctrlLast==WeaponAction::WeaponBow && pl->currentRangedWeapon()==nullptr)
          wctrlLast = WeaponAction::WeaponMele;
        wctrl[wctrlLast] = true;
        }
      return;
      }
    if(ak>=0)
      return; // the windows / map / labels: MainWindow
    if(a>=Action::WeaponMage3 && a<=Action::WeaponMage10) {
      // 3-0: cast the spell of that slot at the cursor (Gothic: draw the rune, then cast with the action key)
      arpgSpellKey(uint8_t(a-Action::WeaponMage3+3), true);
      return;
      }
    if(a==KeyCodec::ActionGeneric && key==Tempest::KeyEvent::K_LControl) {
      // Ctrl is only a modifier for the mouse here (attack anyone / in place; with a bow: aim at the cursor),
      // not Gothic's action key (it would aim the bow and shoot only on release)
      ctrl[a] = true;
      return;
      }
    // Gothic's keys that do the same as ours or break the view from above
    if(a==Action::Walk || a==Action::Sneak || a==Action::Weapon || a==Action::WeaponMele || a==Action::WeaponBow ||
       a==Action::FirstPerson || a==Action::LookBack)
      return;
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

    // gothic-diablo: jump key with a weapon in hand = dodge (side step / jump back)
    if(a==Action::Jump && ws!=WeaponState::NoWeapon && c!=nullptr && c->isArpgActive()) {
      arpgDodge();
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
    if(arpgKeys)
      return; // gothic-diablo: Shift does not toggle walking in the ARPG view
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
  if(a>=Action::WeaponMage3 && a<=Action::WeaponMage10)
    arpgSpellKey(uint8_t(a-Action::WeaponMage3+3), false); // gothic-diablo: end investing

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
  auto set = [pressed](AxisStatus& ax, std::array<bool, KeyCodec::NumMappings>& side, size_t id, float dir) {
    side[id] = pressed;
    if(pressed)
      ax.lastPressed = dir;
    };
  if (action == Action::Forward)
    set(movement.forwardBackward, movement.forwardBackward.main, mappingIndex, 1.f);
  else if (action == Action::Back)
    set(movement.forwardBackward, movement.forwardBackward.reverse, mappingIndex, -1.f);
  else if (action == Action::Right)
    set(movement.strafeRightLeft, movement.strafeRightLeft.main, mappingIndex, 1.f);
  else if (action == Action::Left)
    set(movement.strafeRightLeft, movement.strafeRightLeft.reverse, mappingIndex, -1.f);
  else if (action == Action::RotateR)
    set(movement.turnRightLeft, movement.turnRightLeft.main, mappingIndex, 1.f);
  else if (action == Action::RotateL)
    set(movement.turnRightLeft, movement.turnRightLeft.reverse, mappingIndex, -1.f);
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
      setTarget(arpgClick.target.npc);
    else if(arpgClick.kind==ArpgClick::Strike)
      setTarget(nullptr); // shooting at a point: straight ahead
    else
      setTarget(currentFocus.npc);
    // the enemy under the cursor and the one being attacked glow a little (shader: scene.arpgHi)
    {
      auto glow = [this](const Npc* n) {
        if(n==nullptr || n->isDead() || !arpgIsEnemy(*n))
          return Tempest::Vec4();
        const auto  b  = n->bounds();
        const auto  sz = b.bbox[1]-b.bbox[0];
        const float r  = std::clamp(0.5f*std::max(sz.x, std::max(sz.y, sz.z)), 60.f, 500.f);
        const auto  c  = n->centerPosition();
        return Tempest::Vec4(c.x, c.y, c.z, r);
        };
      const Npc* hov = arpgHover.npc;
      const Npc* tgt = (arpgClick.kind==ArpgClick::Attack) ? arpgClick.target.npc : nullptr;
      camera->arpgSetHighlight(0, glow(hov));
      camera->arpgSetHighlight(1, tgt!=hov ? glow(tgt) : Tempest::Vec4());
    }
    return;
    }
  if(camera!=nullptr) {
    camera->arpgSetHighlight(0, Tempest::Vec4());
    camera->arpgSetHighlight(1, Tempest::Vec4());
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

void PlayerControl::arpgKeyUp(int ak) {
  if(ak>=0 && ak<int(ArpgKeys::Count))
    arpgHeld[ak] = false;
  }

void PlayerControl::clearInput() {
  movement.reset();
  std::memset(arpgHeld,0,sizeof(arpgHeld));
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

  // gothic-diablo: cooldowns count real playing time (a world tick would jump on loading a game)
  arpgSpecialCd = std::max(0.f, arpgSpecialCd-dtF);
  arpgPushCd    = std::max(0.f, arpgPushCd-dtF);
  arpgRollCd    = std::max(0.f, arpgRollCd-dtF);
  for(auto& c:arpgSkillCd)
    c = std::max(0.f, c-dtF);
  if(!arpgSecondRoll) {
    arpgSecondT -= dtF;
    if(arpgSecondT<=0.f)
      arpgSecondRoll = true; // Drugi oddech: ready again
    }
  if(pl!=nullptr && pl->arpgTakeSpecialKill() && heroPerks().battleRush)
    arpgSpecialCd = 0; // Pęd bitewny: a kill with the charged attack
  if(arpgSlashFx.time>w->tickCount()+1000)
    arpgSlashFx = ArpgSlash(); // a game was loaded

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
  arpgTickRain(*pl, dtF); // Diathic: Deszcz strzał goes on by itself

  // gothic-diablo: the camera turn keys of ArpgKeys (arrows) rotate the ARPG camera, whatever the hero is doing
  if(camera!=nullptr && camera->isArpgActive() && !dlg.isActive()) {
    const float camSpeed = 120.f*dtF;
    if(arpgHeld[ArpgKeys::CamLeft])
      camera->arpgRotate(+camSpeed);
    if(arpgHeld[ArpgKeys::CamRight])
      camera->arpgRotate(-camSpeed);
    }

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
  // gothic-diablo: isometric ARPG controls (screen-relative movement)
  auto  camera    = Gothic::inst().camera();
  bool  arpg      = (camera!=nullptr && camera->isArpgActive());
  bool  allowRot  = (arpg || !ctrl[KeyCodec::ActionGeneric]) && pl.isRotationAllowed();

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

  // gothic-diablo: drinking done: draw the weapon again
  if(arpg && arpgRedraw!=0 && arpgPotionReq==0 && ws==WeaponState::NoWeapon && bs!=BS_ITEMINTERACT) {
    if(pl.canSwitchWeapon()) {
      wctrl[arpgRedraw] = true;
      arpgRedraw        = 0;
      }
    }
  else if(arpgRedraw!=0 && (!arpg || ws!=WeaponState::NoWeapon)) {
    arpgRedraw = 0;
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

  // gothic-diablo: dodge
  if(arpg) {
    const float dtF = float(dt)/1000.f;
    if(arpgDodgeLock>0.f) {
      const auto& pk = heroPerks();
      if(arpgSkillNow!=SkNone)
        arpgSkillEnd(pl); // Diathic: a roll out of a skill
      arpgRiposteT   = 0;
      arpgDodgeLock -= dtF;
      runAngleDest   = 0;
      if(arpgRollTotal>0.f)
        arpgTickRoll(pl, dtF);
      const float done = arpgRollTotal>0.f ? 1.f-arpgDodgeLock/arpgRollTotal : 1.f;
      // Drugi oddech: Space again in the second half of a roll = the next roll at once (then 3 s to get it back)
      if(arpgDodgeReq && pk.secondWind && arpgSecondRoll && done>0.5f && arpgStartRoll(pl, rot)==1)
        return;
      // Cios z przewrotu: a click during the roll: the hit comes at the end of it, not after it blends into running
      if(pk.rollStrike && done>0.65f && (arpgClick.kind==ArpgClick::Attack || arpgClick.kind==ArpgClick::Strike))
        arpgDodgeLock = 0.f;
      if(arpgDodgeLock<=0.f)
        arpgEndRoll(pl);
      return;
      }
    if(arpgRollTotal>0.f)
      arpgEndRoll(pl);
    if(arpgDodgeReq) {
      arpgDodgeWait -= dtF;
      if(arpgDodgeWait<=0.f || casting || pl.isFalling() || pl.isInAir() || pl.isJump() ||
         pl.isSwim() || pl.isDive() || pl.isInWater())
        arpgDodgeReq = false;
      else if(arpgStartDodge(pl,rot))
        return;
      }
    } else {
    arpgDodgeReq  = false;
    arpgDodgeLock = 0;
    }

  // Diathic: the skills of the weapon trees (Q / E / F) and Riposta's counter swing
  if(arpg) {
    const float dtF = float(dt)/1000.f;
    if(arpgSkillRun(pl, rot, dtF))
      return;
    if(arpgSkillReq>=0) {
      arpgSkillWait -= dtF;
      if(arpgSkillWait<=0.f || casting || pl.isFalling() || pl.isInAir() || pl.isJump() ||
         pl.isSwim() || pl.isDive() || pl.isInWater()) {
        arpgSkillReq  = -1;
        arpgSkillDraw = false;
        }
      else if(arpgSkillStart(pl, rot)) {
        return;
        }
      }
    } else if(arpgSkillNow!=SkNone) {
    arpgSkillEnd(pl);
    }

  // gothic-diablo: potion keys 1 / 2: drunk at once, weapon stays in hand
  if(arpg && bs==BS_ITEMINTERACT) {
    runAngleDest = 0;
    return; // drinking
    }
  if(arpg && arpgPotionReq!=0) {
    arpgPotionWait -= float(dt)/1000.f;
    int32_t     total = 0;
    const Item* itm   = arpgPickPotion(pl, arpgPotionReq, total);
    if(itm==nullptr || arpgPotionWait<=0.f || casting || pl.isFalling() || pl.isInAir() || pl.isJump() ||
       pl.isSwim() || pl.isDive() || pl.isInWater()) {
      arpgPotionReq = 0;
      }
    else {
      // like Diablo: works at once, the weapon stays in hand (no Gothic drinking animation)
      if(pl.arpgDrinkNow(itm->clsId()))
        arpgPotionReq = 0;
      }
    }

  // gothic-diablo: spell keys 3-0: the rune / scroll of that slot comes out (weapon away at once, rune with Gothic's
  // animation) and is cast towards the cursor (enemy under it = target); key held = invest (charged spells)
  if(arpg && arpgSpellCasting) {
    arpgClick = ArpgClick();
    Tempest::Vec3 aim;
    if(arpgCursorAim(pl, aim) && pl.isRotationAllowed()) {
      const auto  d    = aim-pl.position();
      const float face = std::atan2(d.z, d.x)*180.f/float(M_PI);
      const float turn = 360.f*(float(dt)/1000.f);
      if(d.x*d.x+d.z*d.z>30.f*30.f) {
        rot += std::clamp(std::remainder(face-rot, 360.f), -turn, turn);
        pl.setDirection(rot);
        }
      }
    if(!arpgSpellHeld || ws!=WeaponState::Mage || bs!=BS_CASTING) {
      arpgSpellCasting = false;
      pl.endCastSpell(true);
      }
    runAngleDest = 0;
    return;
    }
  if(arpg && arpgSpellSlot!=0) {
    const float dtF = float(dt)/1000.f;
    arpgSpellWait  -= dtF;
    const Item* spl = pl.inventory().currentSpell(uint8_t(arpgSpellSlot-3));
    if(spl==nullptr || arpgSpellWait<=0.f || pl.isFalling() || pl.isInAir() || pl.isJump() ||
       pl.isSwim() || pl.isDive() || pl.isInWater()) {
      arpgSpellSlot = 0;
      }
    else if(!pl.isAttackAnim()) {
      arpgClick = ArpgClick();
      if(arpgCharge!=ChargeNone)
        arpgChargeCancel(pl);
      if(ws!=WeaponState::Mage || pl.inventory().currentSpellSlot()!=arpgSpellSlot) {
        if(ws!=WeaponState::NoWeapon && ws!=WeaponState::Mage) {
          pl.stopAnim("");
          pl.closeWeapon(true); // the weapon goes away at once
          }
        if(pl.weaponState()!=WeaponState::Mage || pl.inventory().currentSpellSlot()!=arpgSpellSlot) {
          if(pl.canSwitchWeapon())
            pl.drawMage(arpgSpellSlot); // rune / scroll in hand, Gothic's animation
          }
        arpgSpellTurn = 0;
        runAngleDest  = 0;
        return;
        }
      // rune in hand: face the cursor, then cast
      Tempest::Vec3 aim;
      float         da = 0;
      if(arpgCursorAim(pl, aim) && pl.isRotationAllowed()) {
        const auto  d    = aim-pl.position();
        if(d.x*d.x+d.z*d.z>30.f*30.f) {
          const float face = std::atan2(d.z, d.x)*180.f/float(M_PI);
          const float turn = 900.f*dtF;
          da   = std::remainder(face-rot, 360.f);
          rot += std::clamp(da, -turn, turn);
          pl.setDirection(rot);
          da   = std::remainder(face-rot, 360.f);
          }
        }
      arpgSpellTurn += dtF;
      if(bs==BS_STAND && (std::fabs(da)<20.f || arpgSpellTurn>0.4f)) {
        switch(pl.beginCastSpell()) {
          case Npc::BC_Invest:
            arpgSpellCasting = true; // released at once: ends next frame with the lowest level
            arpgSpellSlot    = 0;
            break;
          case Npc::BC_Cast:
            arpgSpellSlot = 0;
            break;
          case Npc::BC_NoMana:
            Gothic::inst().onPrint("Brak many");
            arpgSpellSlot = 0;
            break;
          case Npc::BC_No:
            break; // not ready yet (rune still coming out)
          }
        }
      runAngleDest = 0;
      return;
      }
    }

  // gothic-diablo: middle mouse button with a bow: push the enemies in front away, no damage, 5 s cooldown
  if(arpg && arpgPushReq) {
    arpgPushWait -= float(dt)/1000.f;
    if(arpgPushWait<=0.f || arpgPushCd>0.f || (ws!=WeaponState::Bow && ws!=WeaponState::CBow)) {
      arpgPushReq = false;
      }
    else {
      arpgPushReq = false;
      arpgClick   = ArpgClick();
      if(arpgCharge!=ChargeNone)
        arpgChargeCancel(pl);
      const float face = arpgFightFace(pl, rot);
      if(pl.isRotationAllowed()) {
        rot = face;
        pl.setDirection(rot);
        }
      const float rad = rot*float(M_PI)/180.f;
      const float fx  = std::cos(rad), fz = std::sin(rad);
      const auto  pos = pl.position();
      w->detectNpc(pos, 400.f, [&](Npc& n) {
        if(&n==&pl || n.isDown() || !arpgIsEnemy(n))
          return;
        const auto  d = n.position()-pos;
        const float l = std::sqrt(d.x*d.x+d.z*d.z);
        if(l>300.f)
          return;
        if(l>80.f && (d.x*fx+d.z*fz)/l<0.5f)
          return; // 120 degrees in front
        n.arpgKnockBack(pl, 0.5f); // about 2 m
        });
      pl.emitSoundEffect("WHOOSH", 1500.f, true);
      arpgPushCd        = PushCooldown;
      arpgSlashFx.time  = w->tickCount();
      arpgSlashFx.pos   = pos;
      arpgSlashFx.ang   = rot;
      arpgSlashFx.dir   = 1;
      arpgSlashFx.kind  = 2;
      runAngleDest      = 0;
      return;
      }
    }

  // gothic-diablo: charged attack (right mouse button held): the swing waits in its wind-up while charging;
  // released within 0.2 s after a full 2 s charge = one 180 degree swing with x3 damage that knocks down; 5 s cooldown
  if(arpg && arpgCharge!=ChargeNone) {
    const float dtF = float(dt)/1000.f;
    const bool  melee  = (ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H);
    const bool  ranged = (ws==WeaponState::Bow || ws==WeaponState::CBow);
    const bool  noWeapon = (ws==WeaponState::NoWeapon || ws==WeaponState::Mage); // a rune in hand: weapon first
    if(arpgBlocking || (!melee && !ranged && !noWeapon)) {
      arpgChargeCancel(pl);
      }
    else if(ranged) {
      // bow / crossbow: draw (aim) while charging, the arrow goes off on release
      const float face = arpgFightFace(pl, rot);
      if(pl.isRotationAllowed()) {
        rot = face;
        pl.setDirection(rot);
        }
      arpgClick = ArpgClick();
      if(!pl.hasAmmunition()) {
        Gothic::inst().onPrint("Brak amunicji");
        arpgCharge = ChargeNone;
        }
      else if(arpgCharge==ChargeWant) {
        arpgChargeWait -= dtF;
        const auto bs = pl.bodyStateMasked();
        if(!arpgChargeHeld || arpgChargeWait<=0.f)
          arpgCharge = ChargeNone;
        else if(bs==BS_AIMNEAR || bs==BS_AIMFAR) {
          arpgCharge  = ChargeHold;
          arpgChargeT = 0;
          }
        pl.aimBow();
        return;
        }
      else if(arpgCharge==ChargeHold) {
        arpgChargeT += dtF;
        pl.aimBow();
        const float full = arpgChargeTimeFull(), win = arpgChargeTimeWindow();
        if(arpgChargeHeld && arpgChargeT<=full+win)
          return;
        arpgChargeFull = !arpgChargeHeld && arpgChargeT>=full && arpgChargeT<=full+win;
        arpgChargeLate = !arpgChargeFull && arpgChargeT>full+win && heroPerks().sureHand; // Pewna ręka
        arpgCharge     = ChargeFire;
        arpgChargeWait = 0.6f;
        }
      if(arpgCharge==ChargeFire) {
        arpgChargeWait -= dtF;
        if(arpgChargeFull)
          pl.arpgSetShotMul(heroPerks().chargeMul); // Miażdżący cios
        else if(arpgChargeLate)
          pl.arpgSetShotMul(2.f);
        const bool shot = pl.shootBow();
        pl.arpgSetShotMul(1.f);
        if(shot) {
          if(arpgChargeLate)
            arpgSpecialCd = arpgSpecialCooldown();
          arpgChargeLate = false;
          if(arpgChargeFull) {
            arpgSpecialCd = arpgSpecialCooldown();
            arpgSlashFx.time = w->tickCount();
            arpgSlashFx.pos  = pl.position();
            arpgSlashFx.ang  = rot;
            arpgSlashFx.dir  = 1;
            arpgSlashFx.kind = 1;
            }
          arpgCharge = ChargeNone;
          }
        else if(arpgChargeWait<=0.f) {
          arpgCharge = ChargeNone;
          }
        else {
          pl.aimBow();
          }
        return;
        }
      }
    else if(arpgCharge==ChargeWant) {
      arpgChargeWait -= dtF;
      if(!arpgChargeHeld || arpgChargeWait<=0.f) {
        arpgCharge = ChargeNone;
        }
      else if(noWeapon) {
        arpgDrawPreferred(pl); // draw the weapon first (a rune goes back)
        arpgChargeWait = std::max(arpgChargeWait, 0.8f);
        }
      else {
        const float face = arpgFightFace(pl, rot);
        if(pl.hasState(BS_PARADE))
          pl.stopAnim("");
        if(pl.isRotationAllowed()) {
          rot = face;
          pl.setDirection(rot);
          }
        bool ok = false;
        if(ws==WeaponState::Fist) {
          ok = pl.fistShoot();
          arpgChargeSide = 1;
          }
        else if(pl.swingSwordL()) {
          ok = true;
          arpgChargeSide = 1;
          }
        else if(pl.swingSwordR()) {
          ok = true;
          arpgChargeSide = -1;
          }
        if(ok) {
          arpgCharge  = ChargeHold;
          arpgChargeT = 0;
          arpgClick   = ArpgClick();
          pl.arpgHoldAttack();
          return;
          }
        }
      }
    else if(arpgCharge==ChargeHold) {
      arpgChargeT += dtF;
      arpgClick    = ArpgClick();
      const float face = arpgFightFace(pl, rot);
      if(pl.isRotationAllowed()) {
        const float turn = 720.f*dtF;
        rot += std::clamp(std::remainder(face-rot, 360.f), -turn, turn);
        pl.setDirection(rot);
        }
      const float tFull = arpgChargeTimeFull(), tWin = arpgChargeTimeWindow();
      if(arpgChargeHeld && arpgChargeT<=tFull+tWin) {
        pl.arpgHoldAttack(); // keep charging, standing
        return;
        }
      // released (or held too long): the swing goes on from the wind-up
      const bool full = !arpgChargeHeld && arpgChargeT>=tFull && arpgChargeT<=tFull+tWin;
      if(!full && arpgChargeT>tFull+tWin && heroPerks().sureHand) {
        pl.arpgStartSpecial(2.f, false); // Pewna ręka: late = x2, does not throw down
        arpgSpecialCd = arpgSpecialCooldown();
        }
      if(full) {
        pl.arpgStartSpecial(heroPerks().chargeMul); // Miażdżący cios
        arpgSpecialCd     = arpgSpecialCooldown();
        arpgSlashFx.time  = w->tickCount();
        arpgSlashFx.pos   = pl.position();
        arpgSlashFx.ang   = rot;
        arpgSlashFx.dir   = arpgChargeSide;
        arpgSlashFx.kind  = 0;
        }
      arpgCharge = ChargeNone;
      return;
      }
    }

  // gothic-diablo: block (middle mouse button held): face the enemy and hold the parry until released
  if(arpg && arpgBlocking) {
    if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H) {
      arpgClick = ArpgClick(); // blocking stops walking/attacking
      const float face = arpgFightFace(pl, rot);
      if(pl.isRotationAllowed()) {
        const float turn = 1440.f*(float(dt)/1000.f);
        rot += std::clamp(std::remainder(face-rot, 360.f), -turn, turn);
        pl.setDirection(rot);
        }
      if(!pl.hasState(BS_PARADE)) {
        if(ws==WeaponState::Fist)
          pl.blockFist(); else
          pl.blockSword();
        }
      pl.arpgHoldParade();
      return;
      }
    }

  // gothic-diablo: Ctrl held with a bow / crossbow: draw it and aim at the cursor; Ctrl + click shoots at once
  if(arpg && ctrl[KeyCodec::ActionGeneric] && arpgClick.kind==ArpgClick::None &&
     (ws==WeaponState::Bow || ws==WeaponState::CBow) && pl.hasAmmunition()) {
    Tempest::Vec3 aim;
    bool          hasAim = false;
    if(arpgHover.npc!=nullptr && arpgHover.npc!=&pl && !arpgHover.npc->isDown()) {
      aim    = arpgHover.npc->position();
      hasAim = true;
      }
    else if(arpgGroundValid) {
      aim    = arpgGround;
      hasAim = true;
      }
    if(hasAim && pl.isRotationAllowed()) {
      const auto d = aim-pl.position();
      if(d.x*d.x+d.z*d.z>30.f*30.f) {
        const float face = std::atan2(d.z, d.x)*180.f/float(M_PI);
        const float turn = 720.f*(float(dt)/1000.f);
        rot += std::clamp(std::remainder(face-rot, 360.f), -turn, turn);
        pl.setDirection(rot);
        }
      }
    pl.aimBow();
    return;
    }

  int rotation = 0;
  if(arpg) {
    // turn keys rotate the ARPG camera (PlayerControl::tickMove); mouse does not rotate the hero
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
        case ArpgAttack: ani = Npc::Anim::NoAnim; rotation = 0; break;
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

float PlayerControl::arpgCeiling() const {
  // indoors everything above this height is cut away in the ARPG view: ignore it when picking
  if(auto c = Gothic::inst().camera()) {
    const auto& a = c->arpgView();
    if(a.enabled && a.cutHeight>0.f)
      return a.feet.y + a.cutHeight;
    }
  return std::numeric_limits<float>::max();
  }

Focus PlayerControl::arpgPick() {
  arpgGroundValid = false;

  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  auto cm = Gothic::inst().camera();
  if(w==nullptr || pl==nullptr || cm==nullptr || !arpgCursor.valid)
    return Focus();

  auto&         physic = *w->physic();
  Tempest::Vec3 from   = arpgCursor.rayFrom;
  Tempest::Vec3 dir    = arpgCursor.rayTo - from;
  float         len    = dir.length();
  if(len<1.f)
    return Focus();
  dir = dir/len;
  len = std::min(len, 8000.f);

  // start the ray where it crosses the cut-away plane (upper floors are invisible)
  const float ceiling = arpgCeiling();
  if(from.y>ceiling && dir.y<0.f) {
    const float t = (from.y-ceiling)/(-dir.y);
    if(t<len) {
      from += dir*t;
      len  -= t;
      }
    }
  const Tempest::Vec3 to = from + dir*len;

  const auto land = physic.ray(from, to);
  if(land.hasCol) {
    arpgGround      = land.v;
    arpgGroundValid = true;
    }

  const Tempest::Vec3 center = pl->position();
  const float         radius = cm->arpgView().fowRadius>0.f ? cm->arpgView().fowRadius : 2500.f;
  const float         scale  = std::max(arpgCursor.screen.y, 1.f)/1080.f;
  auto screenDist = [this](const Tempest::Vec3& p) {
    Tempest::Vec4 v = {p.x, p.y, p.z, 1.f};
    arpgCursor.viewProj.project(v);
    if(v.w<=0.f)
      return 1e9f;
    const float sx = (v.x/v.w*0.5f+0.5f)*arpgCursor.screen.x;
    const float sy = (v.y/v.w*0.5f+0.5f)*arpgCursor.screen.y;
    return std::hypot(sx-arpgCursor.pixel.x, sy-arpgCursor.pixel.y);
    };
  auto inFog = [&](const Tempest::Vec3& p) {
    const auto d = p-center;
    return d.x*d.x+d.z*d.z <= radius*radius;
    };

  // 1. creature under the cursor (collision capsule, in front of any wall);
  //    a dead one only if there is no loot lying around it (checked below)
  Npc* corpse = nullptr;
  {
    auto hit = physic.rayNpc(from, to, pl);
    if(hit.npcHit!=nullptr && hit.npcHit!=pl && inFog(hit.npcHit->position()) && hit.npcHit->position().y<=ceiling) {
      if(!hit.npcHit->isDead())
        return Focus(*hit.npcHit);
      corpse = hit.npcHit;
      }
  }

  // 2. items and usable objects near the cursor, only what the hero can see (same rules as the Z labels)
  struct Cand {
    Item*        item = nullptr;
    Interactive* mob  = nullptr;
    float        d    = 0;
    };
  std::vector<Cand> cand;
  w->forEachItemNear(center, radius, [&](Item& it) {
    const auto p = it.midPosition();
    if(p.y>ceiling)
      return;
    const float d = screenDist(p);
    if(d<45.f*scale)
      cand.push_back({&it, nullptr, d-8.f*scale}); // items win close calls
    });
  w->forEachInteractiveNear(center, radius, [&](Interactive& m) {
    if(m.focusName().empty() || m.position().y>ceiling)
      return;
    float d = screenDist(m.displayPosition());
    if(land.vob==&m)
      d = -1.f; // the cursor is right on it
    if(d<50.f*scale)
      cand.push_back({nullptr, &m, d});
    });
  std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b){ return a.d<b.d; });
  for(size_t i=0; i<cand.size() && i<6; ++i) {
    auto& c = cand[i];
    if(c.item!=nullptr && arpgCanSee(c.item->midPosition(), c.item->midPosition().y, nullptr))
      return Focus(*c.item);
    if(c.mob!=nullptr && arpgCanSee(c.mob->displayPosition(), c.mob->position().y, c.mob))
      return Focus(*c.mob);
    }

  if(corpse!=nullptr)
    return Focus(*corpse);

  // 3. creature near the cursor (small or fast ones are hard to hit with a ray);
  //    a living enemy is caught from further away (aim assist: a click just next to it attacks it)
  {
    Npc*  best  = nullptr;
    float bestD = 1e9f;
    for(uint32_t i=0; i<w->npcCount(); ++i) {
      auto npc = w->npcById(i);
      if(npc==nullptr || npc==pl)
        continue;
      const auto p = npc->position();
      if(p.y>ceiling || !inFog(p))
        continue;
      const bool  enemy = !npc->isDown() && arpgIsEnemy(*npc);
      float       d     = std::min(screenDist(npc->centerPosition()), screenDist(p+Tempest::Vec3(0,40.f,0)));
      float       limit = (enemy ? 95.f : 50.f)*scale;
      if(enemy && arpgGroundValid) {
        // cursor on the ground next to it
        const auto g = arpgGround-p;
        if(g.x*g.x+g.z*g.z < 140.f*140.f && std::abs(g.y)<200.f)
          d = std::min(d, 30.f*scale);
        }
      if(enemy)
        d *= 0.75f; // enemies win over bystanders
      if(d<limit && d<bestD && arpgCanSee(npc->centerPosition(), p.y, nullptr)) {
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
  arpgMouseDown(arpgHover);
  }

void PlayerControl::arpgMouseDown(const Focus& f) {
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  const ArpgClick prev = arpgClick;
  arpgClick      = ArpgClick();
  arpgClick.held = true;
  if(pl==nullptr)
    return;

  // a click during a melee swing that hits nothing: the next hit on the same enemy (queued for the combo),
  // not a walk away from the fight
  if(f.npc==nullptr && f.item==nullptr && f.interactive==nullptr && prev.kind==ArpgClick::Attack &&
     prev.target.npc!=nullptr && !prev.target.npc->isDown() && pl->isAttackAnim()) {
    const auto ws = pl->weaponState();
    const auto d  = (arpgGroundValid ? arpgGround : prev.target.npc->position()) - prev.target.npc->position();
    if((ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H) && d.x*d.x+d.z*d.z < 400.f*400.f) {
      arpgClick.kind     = ArpgClick::Attack;
      arpgClick.target   = prev.target;
      arpgClick.stuckPos = pl->position();
      return;
      }
    }

  arpgHover = f;
  const bool force = ctrl[KeyCodec::ActionGeneric]; // Ctrl + click: attack anyone
  if(force && (f.npc==nullptr || f.npc->isDown())) {
    // Ctrl + click on no one: attack in place towards the cursor
    arpgClick.kind = ArpgClick::Strike;
    arpgClick.point = arpgGroundValid ? arpgGround : (pl->position() + Tempest::Vec3(std::cos(pl->rotationRad())*200.f, 0, std::sin(pl->rotationRad())*200.f));
    if(pl->weaponState()==WeaponState::NoWeapon)
      arpgDrawPreferred(*pl);
    arpgClick.stuckPos = pl->position();
    return;
    }
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
    arpgDrawPreferred(*pl); // draw the weapon while running to the enemy
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

namespace {
// Diathic: ArpgNav asks the physics of the world
struct NavScene : ArpgNav::Scene {
  explicit NavScene(World& w):w(w) {}
  bool ray(const Tempest::Vec3& from, const Tempest::Vec3& to, Tempest::Vec3& hit) const override {
    const auto r = w.physic()->ray(from, to);
    hit = r.v;
    return r.hasCol;
    }
  bool sweepDown(const Tempest::Vec3& from, const Tempest::Vec3& to, float r) const override {
    return w.physic()->sweep(from, to, r).hasCol;
    }
  uint64_t    time() const override { return w.tickCount(); }
  const void* id()   const override { return &w; }
  World& w;
  };
}

bool PlayerControl::arpgWalkClear(const Tempest::Vec3& from, const Tempest::Vec3& to) {
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return true;
  return arpgNav.lineClear(NavScene(*w), from, to);
  }

bool PlayerControl::arpgGridPath(Npc& pl, const Tempest::Vec3& dest, float reach) {
  // Diathic: around tables, chairs, benches... on a grid tested with the hero's body (ArpgNav)
  auto  w = Gothic::inst().world();
  auto& c = arpgClick;
  if(w==nullptr)
    return false;
  std::vector<Tempest::Vec3> way;
  if(!arpgNav.findPath(NavScene(*w), pl.position(), dest, reach, way))
    return false;
  c.path     = std::move(way);
  c.pathIdx  = 0;
  c.pathDest = dest;
  c.gridPath = true;
  return true;
  }

Tempest::Vec3 PlayerControl::arpgNextPoint(Npc& pl, const Tempest::Vec3& dest, float reach, uint64_t dt) {
  auto&       c   = arpgClick;
  const auto  pos = pl.position();
  const float dtF = float(dt)/1000.f;

  // a few times a second: straight on while the walk is clear (tested with the hero's body, ArpgNav), else a way
  // around on the grid near the hero; the waynet for far places; the path is shortened to the farthest point
  // that can be walked to straight
  c.repathTimer -= dtF;
  if(c.repathTimer<=0.f || c.replan) {
    c.repathTimer = 0.4f;
    const auto  md       = dest-c.pathDest;
    const bool  destMove = (md.x*md.x+md.z*md.z) > 200.f*200.f;
    const bool  clear    = !c.replan && arpgWalkClear(pos,dest);
    c.clearCount = clear ? c.clearCount+1 : 0;
    c.blockCount = clear ? 0 : c.blockCount+1;
    if(clear) {
      c.path.clear();
      c.pathIdx  = 0;
      c.gridPath = false;
      }
    else if(c.path.empty() || destMove || c.replan) {
      if(destMove || c.replan)
        c.gridFailed = false;
      c.replan   = false;
      c.gridPath = false;
      // the grid costs a little: after it found nothing for this destination it is not asked every 0.4 s
      const bool grid = !c.gridFailed && arpgGridPath(pl, dest, reach);
      if(!grid)
        c.gridFailed = true;
      if(!grid) {
        arpgPlanPath(pl, dest);
        // a far place: the waynet; around things close by: the grid to the next point of it
        if(c.pathIdx<c.path.size() && !arpgWalkClear(pos, c.path[c.pathIdx])) {
          const auto rest = std::vector<Tempest::Vec3>(c.path.begin()+long(c.pathIdx), c.path.end());
          if(arpgGridPath(pl, rest.front(), ArpgNav::Cell)) {
            c.path.insert(c.path.end(), rest.begin()+1, rest.end());
            c.pathDest = dest;
            }
          }
        }
      }
    else {
      arpgPullPath(pos);
      }
    }

  const float near = c.gridPath ? 35.f : 90.f; // the corners of a way around furniture: closely
  while(c.pathIdx<c.path.size()) {
    const auto d = c.path[c.pathIdx]-pos;
    if(d.x*d.x+d.z*d.z > near*near)
      break;
    ++c.pathIdx;
    }

  // no progress for 0.4 s: what is in front of him is closed on the grid, plan again; give up after ~1.6 s
  c.stuckTimer += dtF;
  if(c.stuckTimer>=0.4f) {
    const auto  d      = pos-c.stuckPos;
    const float moved2 = d.x*d.x+d.z*d.z;
    c.stuckTimer = 0;
    c.stuckPos   = pos;
    if(moved2<20.f*20.f) {
      c.stuckCount++;
      c.avoidSide  = (c.avoidSide==0) ? 1 : -c.avoidSide;
      c.detourTime = 0;
      const float a = pl.rotationRad();
      arpgNav.block(pos + Tempest::Vec3(std::cos(a), 0, std::sin(a))*(ArpgNav::Radius+ArpgNav::Cell));
      c.replan = true;
      if(c.stuckCount>=4)
        c.kind = ArpgClick::None;
      }
    else if(moved2>50.f*50.f) {
      c.stuckCount = 0;
      }
    }

  if(c.pathIdx<c.path.size())
    return c.path[c.pathIdx];
  return dest;
  }

bool PlayerControl::arpgFeelerClear(const Npc& pl, float ang, float len) const {
  // two rays at chest height, one per shoulder: low things (benches, crates) are handled by the collision reaction
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return true;
  const float         a    = ang*float(M_PI)/180.f;
  const Tempest::Vec3 dir  = {std::cos(a), 0, std::sin(a)};
  const Tempest::Vec3 side = {-dir.z, 0, dir.x};
  const Tempest::Vec3 base = pl.position() + Tempest::Vec3(0,100.f,0);
  for(float o:{-25.f, 25.f}) {
    const auto from = base + side*o;
    if(w->physic()->ray(from, from+dir*len).hasCol)
      return false;
    }
  return true;
  }

float PlayerControl::arpgFreeDistance(const Npc& pl, float ang) const {
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return 0;
  const float         a    = ang*float(M_PI)/180.f;
  const Tempest::Vec3 dir  = {std::cos(a), 0, std::sin(a)};
  const Tempest::Vec3 from = pl.position() + Tempest::Vec3(0,60.f,0);
  const float         len  = 250.f;
  auto r = w->physic()->ray(from, from+dir*len);
  return r.hasCol ? (r.v-from).length() : len;
  }

float PlayerControl::arpgSteer(Npc& pl, const Tempest::Vec3& next, uint64_t dt) {
  // gothic-diablo: local obstacle avoidance, a bit like a robot vacuum:
  // react to the side of the last collision, look ahead with "feelers", keep to one side
  auto&       c    = arpgClick;
  auto        w    = Gothic::inst().world();
  const auto  pos  = pl.position();
  const float dtF  = float(dt)/1000.f;
  const auto  d    = next-pos;
  const float dist = std::sqrt(d.x*d.x+d.z*d.z);
  const float goal = std::atan2(d.z, d.x)*180.f/float(M_PI);
  float       want = goal;

  // 1. just bumped into something: go along it, on the side that is closer to where we want to go
  const auto& mf = pl.lastMoveFail();
  if(mf.time!=0 && mf.time!=c.lastFail && mf.time+250>=w->tickCount()) {
    c.lastFail = mf.time;
    float nx = mf.normal.x, nz = mf.normal.z;
    const float nl = std::sqrt(nx*nx+nz*nz);
    if(nl>0.1f) {
      nx/=nl; nz/=nl;
      const float a1 = std::atan2(nx, -nz)*180.f/float(M_PI); // tangent of the obstacle
      const float a2 = a1+180.f;
      const float r1 = std::remainder(a1-goal, 360.f);
      const float r2 = std::remainder(a2-goal, 360.f);
      float pick = a1;
      if(c.avoidSide!=0)
        pick = ((r1>=0.f ? 1 : -1)==c.avoidSide) ? a1 : a2;
      else if(std::fabs(std::fabs(r1)-std::fabs(r2))>30.f)
        pick = std::fabs(r1)<std::fabs(r2) ? a1 : a2;
      else
        pick = arpgFreeDistance(pl,a1)>=arpgFreeDistance(pl,a2) ? a1 : a2;
      c.avoidSide  = (std::remainder(pick-goal, 360.f)>=0.f) ? 1 : -1;
      c.detourAng  = pick;
      c.detourTime = mf.npc ? 0.5f : 0.3f;
      }
    }

  // 2. detour in progress
  if(c.detourTime>0.f) {
    c.detourTime -= dtF;
    want = c.detourAng;
    }

  // 3. feelers: something ahead -> turn away from it, preferring the side we already avoid on
  //    (not on a way around furniture from the grid: it is clear already, the feelers would only push him off it)
  const float len = std::min(110.f, dist-20.f);
  if(!c.gridPath && len>=40.f && !arpgFeelerClear(pl, want, len)) {
    const int s0 = (c.avoidSide!=0) ? c.avoidSide : 1;
    for(float off:{30.f, 60.f, 90.f}) {
      for(int side:{s0, -s0}) {
        const float a = want + float(side)*off;
        if(arpgFeelerClear(pl, a, len)) {
          c.avoidSide = side;
          return a;
          }
        }
      }
    }
  return want;
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

  if(c.kind==ArpgClick::Strike) {
    c.target = w->validateFocus(c.target);
    if(c.target.npc!=nullptr && c.target.npc->isDown())
      c.target = Focus();
    }
  else if(c.kind!=ArpgClick::Ground) {
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
      reach = 180.f;
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
    case ArpgClick::Strike:
      if(c.held && arpgGroundValid)
        c.point = arpgGround; // holding the button: keep hitting towards the cursor
      dest  = (c.target.npc!=nullptr && !c.held) ? c.target.npc->position() : c.point;
      reach = 1e6f;           // never walks
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
      // a bit closer than the reach of the hit (Npc::commitDamageArpg): the swing lands even if the enemy steps back
      inRange = pl.arpgInReach(*c.target.npc, 0.9f);
      }
    else if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
      // bows: close enough and a free line of fire
      const float rng = arpgShotRange();
      inRange = dist2<=rng*rng && arpgLineClear(pos, dest);
      }
    }

  if(!inRange) {
    const auto kind = c.kind;
    // how close the way has to lead: a bit less than what counts as "in range" (Diathic: ArpgNav)
    float navReach = ArpgNav::Cell;
    switch(kind) {
      case ArpgClick::PickUp: navReach = reach*0.8f; break;
      case ArpgClick::Use:    navReach = reach*0.8f; break;
      case ArpgClick::Talk:   navReach = reach*0.8f; break;
      case ArpgClick::Loot:   navReach = reach*0.8f; break;
      case ArpgClick::Attack: navReach = 110.f;      break;
      default:                navReach = ArpgNav::Cell; break;
      }
    arpgNextPoint(pl, dest, navReach, dt);
    if(c.kind==ArpgClick::None) {
      // stuck (furniture, shelves): close enough still counts for using things
      if(kind!=ArpgClick::Ground && kind!=ArpgClick::Attack && dist2<=320.f*320.f) {
        c.kind  = kind;
        inRange = true;
        } else {
        return ArpgStop;
        }
      }
    }
  if(!inRange) {
    const auto next = (c.path.size()>c.pathIdx) ? c.path[c.pathIdx] : dest;
    const float ang  = arpgSteer(pl, next, dt);
    const float da   = std::remainder(ang-rot, 360.f);
    if(allowRot)
      rot += std::clamp(da, -step, step);
    if(std::fabs(da)<100.f)
      return ArpgMove;
    rotation = (da>0.f) ? -1 : 1;
    return ArpgTurn;
    }

  // in range: face the target (fast in a fight)
  float faceDa  = 0;
  float faceAng = rot;
  if(c.kind!=ArpgClick::Ground && dist2>1.f) {
    const bool  fight = (c.kind==ArpgClick::Attack || c.kind==ArpgClick::Strike);
    const float turn  = fight ? step*1.6f : step;
    faceAng = std::atan2(d.z, d.x)*180.f/float(M_PI);
    faceDa  = std::remainder(faceAng-rot, 360.f);
    if(allowRot && !pl.arpgSpinning()) // Wir: the turn round is not stopped
      rot += std::clamp(faceDa, -turn, turn);
    }

  switch(c.kind) {
    case ArpgClick::None:
      break;
    case ArpgClick::Ground:
      if(c.held)
        return ArpgStop; // button still down: wait for the cursor to move
      break;
    case ArpgClick::PickUp:
      // also with a weapon in hand (Npc::takeItem skips the animation then);
      // the hero may still be running/turning: keep trying for a moment
      if(!pl.isDown() && pl.takeItem(*c.target.item)==nullptr) {
        c.actTimer += float(dt)/1000.f;
        if(c.actTimer<1.5f)
          return ArpgStop;
        }
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
      if(c.held && c.swings>0 && (tg.isDead() || tg.isDown())) {
        // button still held: the next enemy around (like Diablo)
        const auto ws0  = pl.weaponState();
        const bool rngd = (ws0==WeaponState::Bow || ws0==WeaponState::CBow);
        if(auto next = arpgNextEnemy(pl, &tg, rngd ? arpgShotRange() : 450.f)) {
          c.target = Focus(*next);
          c.swings = 0;
          c.path.clear();
          setTarget(next);
          return ArpgStop;
          }
        break;
        }
      if(tg.isDead())
        break;
      const auto ws = pl.weaponState();
      if(ws==WeaponState::NoWeapon) {
        arpgDrawPreferred(pl);
        return ArpgStop;
        }
      if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
        if(tg.isDown())
          break;
        if(arpgShoot(pl, rot, allowRot, faceAng, &tg))
          return ArpgStop;
        break;
        }
      if(ws==WeaponState::Mage) {
        arpgDrawPreferred(pl); // rune in hand: the mouse fights with the weapon (spells are on 3-0)
        return ArpgStop;
        }
      if(ws!=WeaponState::Fist && ws!=WeaponState::W1H && ws!=WeaponState::W2H)
        break;
      if(tg.isDown()) {
        // unconscious human: finishing move, otherwise leave it
        if(c.swings==0 && !pl.isAttackAnim() && pl.canFinish(tg))
          pl.finishingMove();
        break;
        }
      if(arpgSwing(pl, rot, allowRot, faceAng, faceDa))
        return pl.isAttackAnim() ? ArpgAttack : ArpgStop;
      break; // order done
      }
    case ArpgClick::Strike: {
      const auto ws = pl.weaponState();
      if(ws==WeaponState::NoWeapon) {
        arpgDrawPreferred(pl);
        return ArpgStop;
        }
      if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
        if(arpgShoot(pl, rot, allowRot, faceAng, nullptr))
          return ArpgStop;
        break;
        }
      if(ws==WeaponState::Mage) {
        arpgDrawPreferred(pl);
        return ArpgStop;
        }
      if(ws!=WeaponState::Fist && ws!=WeaponState::W1H && ws!=WeaponState::W2H)
        break;
      if(arpgSwing(pl, rot, allowRot, faceAng, faceDa))
        return pl.isAttackAnim() ? ArpgAttack : ArpgStop;
      break;
      }
    }
  c = ArpgClick();
  return ArpgStop;
  }

void PlayerControl::arpgDodge() {
  arpgDodgeReq  = true;
  arpgDodgeWait = 0.35f; // pressed during an attack: dodge as soon as the swing can be interrupted
  }

int PlayerControl::arpgStartRoll(Npc& pl, float& rot) {
  // gothic-diablo: dodge roll towards the cursor with i-frames (Dark Souls). Gothic II has the roll only as the
  // acrobatics landing (HUMANS_ACROBATIC.MDS, T_FALLDN_2_RUNL, moves ~1.6 m): the overlay is put on for the roll
  // (later the roll becomes a skill to learn)
  static const char* overlay = "HUMANS_ACROBATIC.MDS";
  // 0 = no roll animation (old dodge), 1 = rolling, 2 = not now (just rolled: the request waits a moment)
  auto sk = Resources::loadSkeleton(overlay);
  if(sk==nullptr)
    return 0;
  const auto& pk     = heroPerks();
  const bool  chain  = (arpgDodgeLock>0.f); // Drugi oddech: a roll right out of the last one
  if(arpgRollCd>0.f && !chain)
    return 2;

  // direction: to the cursor, keyboard (W/S/A/D) wins when held
  float dir = pl.rotation();
  Tempest::Vec3 aim;
  if(arpgCursorAim(pl, aim)) {
    const auto d = aim-pl.position();
    if(d.x*d.x+d.z*d.z>40.f*40.f)
      dir = std::atan2(d.z,d.x)*180.f/float(M_PI);
    }
  const float fb = movement.forwardBackward.value();
  const float lr = movement.strafeRightLeft.value();
  if(auto camera = Gothic::inst().camera(); camera!=nullptr && (fb!=0.f || lr!=0.f))
    dir = camera->arpgYaw() + std::atan2(-lr, fb)*180.f/float(M_PI);

  if(!pl.arpgHasOverlay(sk))
    pl.addOverlay(sk, 1600); // ms, the roll takes ~1 s
  if(arpgCharge!=ChargeNone)
    arpgChargeCancel(pl);
  if(pl.hasState(BS_PARADE))
    pl.stopAnim("");
  pl.setAnimRotate(0);
  rot = dir;
  pl.setDirection(rot);
  if(chain)
    pl.stopAnim("T_FALLDN_2_RUNL"); // the same animation would just go on
  auto sq = pl.playAnimByName("T_FALLDN_2_RUNL", BS_RUN);
  if(sq==nullptr)
    return 2; // busy (e.g. a swing that cannot be interrupted): try again
  static constexpr float RollSpeed = 1.4f; // faster than Gothic's landing roll
  pl.arpgPlayFaster(sq, RollSpeed-1.f);
  const float len = sq->totalTime()/1000.f/RollSpeed;
  // the first ~2/3: the roll itself; Zwinność: longer
  pl.arpgSetInvulnerable(uint64_t(std::min(len,1.f)*650.f + pk.rollInvuln*1000.f));
  arpgDodgeReq  = false;
  arpgDodgeLock = std::clamp(len*0.85f, 0.4f, 1.f);         // the end blends into running
  arpgRollCd    = std::max(0.1f, arpgDodgeLock + 0.25f - pk.rollCdCut -   // Lekki krok
                           (pl.weaponState()==WeaponState::W1H ? pk.rollCd1H : 0.f)); // Diathic: Zwinny krok
  arpgClick     = ArpgClick();
  runAngleDest  = 0;
  if(chain) {
    arpgSecondRoll = false;
    arpgSecondT    = 3.f;
    }

  // the skill tree Fikołek during the roll (PlayerControl::arpgTickRoll)
  const float rad = rot*float(M_PI)/180.f;
  arpgRollTotal = arpgDodgeLock;
  arpgRollDir   = Tempest::Vec3(std::cos(rad), 0, std::sin(rad));
  arpgRollExtra = 160.f*pk.rollDist;                        // Dalszy przewrót: the roll is ~1.6 m
  arpgRollTripped.clear();
  if(pk.rollThrough && !arpgRollNoNpc) {
    pl.arpgNpcCollision(false);                             // Przez szereg
    arpgRollNoNpc = true;
    }
  if(auto w = Gothic::inst().world())
    pl.arpgRollDone(w->tickCount()+uint64_t(arpgDodgeLock*1000.f)); // Kontratak, Wyczucie strzał count from its end
  return 1;
  }

void PlayerControl::arpgTickRoll(Npc& pl, float dtF) {
  const auto& pk = heroPerks();
  auto        w  = Gothic::inst().world();
  if(w==nullptr)
    return;
  // Dalszy przewrót: further than the animation
  if(arpgRollExtra>0.f && arpgRollTotal>0.f)
    pl.tryMove(arpgRollDir*(arpgRollExtra*dtF/arpgRollTotal));
  // Piasek w oczy: the enemies passed miss; Podcięcie: the ones in the way fall
  if(pk.sandEyes>0.f || pk.trip) {
    const auto pos = pl.position();
    w->detectNpc(pos, 220.f, [&](Npc& n) {
      if(&n==&pl || n.isDown() || !arpgIsEnemy(n))
        return;
      const auto  d  = n.position()-pos;
      const float l2 = d.x*d.x+d.z*d.z;
      if(pk.sandEyes>0.f && l2<170.f*170.f)
        n.arpgBlind(uint64_t(pk.sandEyes*1000.f));
      if(pk.trip && l2<120.f*120.f && d.x*arpgRollDir.x+d.z*arpgRollDir.z>-20.f &&
         std::find(arpgRollTripped.begin(), arpgRollTripped.end(), &n)==arpgRollTripped.end()) {
        arpgRollTripped.push_back(&n);
        n.arpgKnockDown(pl);
        }
      });
    }
  }

void PlayerControl::arpgEndRoll(Npc& pl) {
  const auto& pk = heroPerks();
  if(arpgRollNoNpc) {
    pl.arpgNpcCollision(true);
    arpgRollNoNpc = false;
    }
  if(pk.shadow)
    pl.arpgSetShadow(1000); // Cień
  // clicks made during the roll: Cios z przewrotu turns them into a hit right after it; without it they are dropped
  if(!pk.rollStrike && (arpgClick.kind==ArpgClick::Attack || arpgClick.kind==ArpgClick::Strike))
    arpgClick = ArpgClick();
  arpgRollTripped.clear();
  arpgRollTotal = 0;
  }

bool PlayerControl::arpgStartDodge(Npc& pl, float& rot) {
  auto camera = Gothic::inst().camera();
  if(camera==nullptr)
    return false;
  switch(arpgStartRoll(pl, rot)) {
    case 1: return true;
    case 2: return false;
    default: break; // no roll in this game: Gothic's side step / jump back
    }

  // reference direction: the enemy we fight, otherwise where the hero looks
  float face = pl.rotation();
  Npc*  tg   = (arpgClick.kind==ArpgClick::Attack) ? arpgClick.target.npc : pl.target();
  if(tg!=nullptr && tg!=&pl && !tg->isDown()) {
    const auto d = tg->position()-pl.position();
    if(d.x*d.x+d.z*d.z > 1.f && d.x*d.x+d.z*d.z < 2000.f*2000.f)
      face = std::atan2(d.z,d.x)*180.f/float(M_PI);
    }

  // dodge direction: W/S/A/D on screen, otherwise away from the enemy
  float dir = face+180.f;
  const float fb = movement.forwardBackward.value();
  const float lr = movement.strafeRightLeft.value();
  if(fb!=0.f || lr!=0.f)
    dir = camera->arpgYaw() + std::atan2(-lr, fb)*180.f/float(M_PI);

  // Gothic has a side step (left/right) and a jump back: pick the one that keeps the hero
  // facing the enemy best, and turn him so that the animation moves exactly towards 'dir'
  struct Opt { Npc::Anim anim; float face; float err; };
  Opt opt[3] = {
    {Npc::Anim::MoveBack, dir+180.f, 0},
    {Npc::Anim::MoveL,    dir- 90.f, 0},
    {Npc::Anim::MoveR,    dir+ 90.f, 0},
    };
  for(auto& o:opt)
    o.err = std::fabs(std::remainder(o.face-face, 360.f));
  std::sort(std::begin(opt), std::end(opt), [](const Opt& a, const Opt& b){ return a.err<b.err; });

  if(arpgCharge!=ChargeNone)
    arpgChargeCancel(pl); // dodging out of a charge
  if(pl.hasState(BS_PARADE))
    pl.stopAnim(""); // dodging out of a block: the held parry would not let the step start
  for(auto& o:opt) {
    auto sq = pl.setAnimAngGet(o.anim);
    if(sq==nullptr)
      continue;
    rot = o.face;
    pl.setAnimRotate(0);
    pl.setDirection(rot);
    arpgDodgeReq  = false;
    arpgDodgeLock = std::clamp(sq->totalTime()/1000.f, 0.25f, 0.9f);
    arpgClick     = ArpgClick(); // dodging cancels the current mouse order
    runAngleDest  = 0;
    return true;
    }
  return false;
  }

bool PlayerControl::arpgCanSee(const Tempest::Vec3& p, float baseY, const Interactive* self) const {
  // in the hero's sight: inside the fog of war, below the cut-away ceiling, not behind a wall/floor
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  auto c  = Gothic::inst().camera();
  if(w==nullptr || pl==nullptr || c==nullptr)
    return false;
  const auto& view   = c->arpgView();
  const float radius = view.fowRadius>0.f ? view.fowRadius : 2500.f;
  const auto  center = pl->position();
  const auto  d      = p-center;
  if(d.x*d.x+d.z*d.z > radius*radius || baseY>arpgCeiling())
    return false;
  const auto eye  = pl->centerPosition() + Tempest::Vec3(0,50.f,0);
  const auto land = w->physic()->ray(eye, p);
  if(!land.hasCol || (self!=nullptr && land.vob==self))
    return true;
  return (land.v-p).quadLength() < 100.f*100.f;
  }

void PlayerControl::arpgVisibleObjects(std::vector<Focus>& out) const {
  out.clear();
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  auto c  = Gothic::inst().camera();
  if(w==nullptr || pl==nullptr || c==nullptr || !c->isArpgActive())
    return;

  const auto& view   = c->arpgView();
  const float radius = view.fowRadius>0.f ? view.fowRadius : 2500.f;
  const auto  center = pl->position();

  w->forEachItemNear(center, radius, [&](Item& it) {
    const auto p = it.midPosition();
    if(arpgCanSee(p, p.y, nullptr))
      out.push_back(Focus(it));
    });
  w->forEachInteractiveNear(center, radius, [&](Interactive& m) {
    if(m.focusName().empty())
      return;
    if(arpgCanSee(m.displayPosition(), m.position().y, &m))
      out.push_back(Focus(m));
    });
  }

bool PlayerControl::arpgCursorOnEnemy() const {
  auto n = arpgHover.npc;
  return n!=nullptr && !n->isDead() && !n->isDown() && arpgIsEnemy(*n);
  }

bool PlayerControl::arpgIsEnemy(const Npc& npc) const {
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  return pl!=nullptr && &npc!=pl && !npc.isDown() && arpgIsHostile(*pl,npc);
  }

bool PlayerControl::arpgAttackQueued() const {
  return arpgClick.kind==ArpgClick::Attack && (arpgClick.held || arpgClick.swings==0);
  }

bool PlayerControl::arpgSwing(Npc& pl, float& rot, bool allowRot, float faceAng, float faceDa) {
  // single click: one attack; click during a swing: queued, chained at the right moment (combo);
  // held button: keep attacking, every hit chained in its combo window.
  // Returns false when the order is done.
  auto&      c    = arpgClick;
  const auto ws   = pl.weaponState();
  if(c.swingCheck) {
    // a swing that did not really start (cut in the same frame, e.g. by stopping from a run) does not count:
    // a single click would end with no hit at all
    c.swingCheck = false;
    if(!pl.isAttackAnim() && c.swings>0)
      c.swings--;
    }
  const bool want = (c.held || c.swings==0);
  if(want && (std::fabs(faceDa)<100.f || pl.isAttackAnim())) {
    bool go = true;
    if(pl.isAttackAnim()) {
      // never press too early (Gothic breaks the combo then): wait for the window or the end of the swing
      const auto ct = pl.comboTiming();
      go = ct.attack && ct.open && !ct.broken;
      }
    if(go) {
      // like Diablo: the hero snaps to the target when he swings
      if(allowRot) {
        rot = faceAng;
        pl.setDirection(rot);
        }
      const bool ok = (ws==WeaponState::Fist) ? pl.fistShoot() : pl.swingSword();
      if(ok) {
        c.swings++;
        c.swingCheck = true;
        }
      }
    return true;
    }
  if(!want && !pl.isAttackAnim())
    return false;
  return true;
  }

void PlayerControl::arpgSwapWeapon() {
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  if(pl==nullptr)
    return;
  const auto ws = pl->weaponState();
  const bool hasBow = pl->currentRangedWeapon()!=nullptr;
  if(ws==WeaponState::Bow || ws==WeaponState::CBow || ws==WeaponState::Mage) {
    wctrl[WeaponMele] = true;
    }
  else if(ws==WeaponState::NoWeapon) {
    if(wctrlLast==WeaponBow || !hasBow)
      wctrl[WeaponMele] = true; else
      wctrl[WeaponBow]  = true;
    }
  else if(hasBow) {
    wctrl[WeaponBow] = true;
    }
  }

void PlayerControl::arpgPlanPath(Npc& pl, const Tempest::Vec3& dest) {
  auto  w   = Gothic::inst().world();
  auto& c   = arpgClick;
  const auto pos = pl.position();
  c.path.clear();
  c.pathIdx  = 0;
  c.pathDest = dest;
  if(auto end = w->findWayPoint(dest)) {
    WayPath way = w->wayTo(pl,*end);
    while(auto wp = way.pop())
      c.path.push_back(wp->position());
    }
  if(c.path.empty())
    return;
  c.path.push_back(dest);
  // the first waypoint is often behind the hero: skip it when the next one is closer
  while(c.pathIdx+1<c.path.size()) {
    const auto a = c.path[c.pathIdx]-pos, b = c.path[c.pathIdx+1]-pos, ab = c.path[c.pathIdx+1]-c.path[c.pathIdx];
    const float da = a.x*a.x+a.z*a.z, db = b.x*b.x+b.z*b.z, dab = ab.x*ab.x+ab.z*ab.z;
    if(db<dab || db<da)
      ++c.pathIdx; else
      break;
    }
  arpgPullPath(pos);
  }

void PlayerControl::arpgPullPath(const Tempest::Vec3& pos) {
  // "string pulling": go straight to the farthest point of the path that can be seen
  auto& c = arpgClick;
  for(size_t k=c.path.size(); k>c.pathIdx+1; --k) {
    if(arpgWalkClear(pos, c.path[k-1])) {
      c.pathIdx = k-1;
      return;
      }
    }
  }

void PlayerControl::arpgBlock(bool on) {
  arpgBlocking = on;
  }

bool PlayerControl::arpgCanBlock() const {
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  if(pl==nullptr)
    return false;
  const auto ws = pl->weaponState();
  return ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H;
  }

float PlayerControl::arpgFightFace(Npc& pl, float def) {
  // whom to face in a fight: the enemy under the cursor, the closest enemy close by, or the cursor
  auto w = Gothic::inst().world();
  if(arpgHover.npc!=nullptr && arpgIsEnemy(*arpgHover.npc)) {
    const auto d = arpgHover.npc->position()-pl.position();
    return std::atan2(d.z,d.x)*180.f/float(M_PI);
    }
  float face = def;
  bool  has  = false;
  if(w!=nullptr) {
    float best = 400.f*400.f;
    w->detectNpc(pl.position(), 400.f, [&](Npc& n) {
      if(!arpgIsEnemy(n))
        return;
      const auto  d  = n.position()-pl.position();
      const float d2 = d.x*d.x+d.z*d.z;
      if(d2<best) {
        best = d2;
        face = std::atan2(d.z,d.x)*180.f/float(M_PI);
        has  = true;
        }
      });
    }
  if(!has && arpgGroundValid) {
    const auto d = arpgGround-pl.position();
    if(d.x*d.x+d.z*d.z>100.f)
      face = std::atan2(d.z,d.x)*180.f/float(M_PI);
    }
  return face;
  }

void PlayerControl::arpgChargeStart() {
  auto w = Gothic::inst().world();
  if(w==nullptr || arpgSpecialCd>0.f)
    return; // still cooling down
  arpgCharge     = ChargeWant;
  arpgChargeHeld = true;
  arpgChargeWait = 1.f; // pressed during a swing: start as soon as the swing allows
  arpgChargeT    = 0;
  }

void PlayerControl::arpgChargeRelease() {
  arpgChargeHeld = false;
  }

void PlayerControl::arpgChargeCancel(Npc& pl) {
  const auto ws = pl.weaponState();
  if(arpgCharge==ChargeHold && ws!=WeaponState::Bow && ws!=WeaponState::CBow)
    pl.stopAnim(""); // drop the frozen wind-up
  arpgCharge = ChargeNone;
  }

bool PlayerControl::arpgShoot(Npc& pl, float& rot, bool allowRot, float faceAng, Npc* tg) {
  // bows: aim at the target (or straight towards the cursor), shoot; held button = keep shooting.
  // Returns false when the order is done.
  auto& c = arpgClick;
  if(!pl.hasAmmunition()) {
    Gothic::inst().onPrint("Brak amunicji");
    wctrl[WeaponMele] = true;
    return false;
    }
  if(allowRot) {
    rot = faceAng; // the aim follows the target
    pl.setDirection(rot);
    }
  setTarget(tg);
  const bool want = (c.held || c.swings==0);
  if(want) {
    const auto bs = pl.bodyStateMasked();
    if((bs==BS_AIMNEAR || bs==BS_AIMFAR) && pl.shootBow())
      c.swings++; else
      pl.aimBow();
    return true;
    }
  return pl.isAttackAnim();
  }

bool PlayerControl::arpgHasRanged(Npc& pl) const {
  auto rgn = pl.currentRangedWeapon();
  if(rgn==nullptr)
    return false;
  const int32_t munition = rgn->handle().munition;
  return munition>=0 && pl.inventory().itemCount(size_t(munition))>0;
  }

void PlayerControl::arpgSpellKey(uint8_t slot, bool pressed) {
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  if(pl==nullptr)
    return;
  if(!pressed) {
    if(arpgSpellSlot==slot || arpgSpellCasting)
      arpgSpellHeld = false;
    return;
    }
  if(pl->inventory().currentSpell(uint8_t(slot-3))==nullptr)
    return; // empty slot (a rune is put there in the inventory: select it and press the key)
  if(arpgSpellCasting)
    return;
  arpgSpellSlot = slot;
  arpgSpellHeld = true;
  arpgSpellWait = 2.f;
  arpgSpellTurn = 0;
  }

Npc* PlayerControl::arpgNextEnemy(Npc& pl, const Npc* except, float range) const {
  // nearest living enemy the hero can see, the ones in front of him first
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return nullptr;
  const auto  pos = pl.position();
  const float rad = pl.rotationRad();
  const float fx  = std::cos(rad), fz = std::sin(rad);
  Npc*  best  = nullptr;
  float bestS = 1e9f;
  w->detectNpc(pos, range, [&](Npc& n) {
    if(&n==&pl || &n==except || n.isDown() || !arpgIsEnemy(n))
      return;
    const auto  d = n.position()-pos;
    const float l = std::sqrt(d.x*d.x+d.z*d.z);
    if(l>range || std::abs(d.y)>300.f)
      return;
    if(!arpgCanSee(n.centerPosition(), n.position().y, nullptr))
      return;
    const float front = (l>1.f) ? (d.x*fx+d.z*fz)/l : 1.f;
    const float score = l*(1.5f-0.5f*front); // behind the hero counts as 2x further away
    if(score<bestS) {
      bestS = score;
      best  = &n;
      }
    });
  return best;
  }

bool PlayerControl::arpgCursorAim(const Npc& pl, Tempest::Vec3& out) const {
  // the enemy (or anyone) under the cursor, otherwise the point on the ground
  if(arpgHover.npc!=nullptr && arpgHover.npc!=&pl && !arpgHover.npc->isDead()) {
    out = arpgHover.npc->position();
    return true;
    }
  if(arpgGroundValid) {
    out = arpgGround;
    return true;
    }
  return false;
  }

void PlayerControl::arpgDrawPreferred(Npc& pl) {
  if(wctrlLast==WeaponBow && arpgHasRanged(pl))
    wctrl[WeaponBow]  = true; else
    wctrl[WeaponMele] = true;
  }

float PlayerControl::arpgShotRange() const {
  float r = 2500.f;
  if(auto c = Gothic::inst().camera()) {
    const auto& v = c->arpgView();
    if(v.fowRadius>0.f)
      r = std::min(r, v.fowRadius);
    }
  return r;
  }

float PlayerControl::arpgSpecialLeft() const {
  return std::clamp(arpgSpecialCd/arpgSpecialCooldown(), 0.f, 1.f);
  }

float PlayerControl::arpgPushLeft() const {
  return std::clamp(arpgPushCd/PushCooldown, 0.f, 1.f);
  }

void PlayerControl::arpgStop() {
  arpgSkillReq   = -1;
  arpgSkillDraw  = false;
  arpgRiposteT   = 0;
  if(arpgSkillNow!=SkNone) {
    auto w = Gothic::inst().world();
    if(auto pl = (w!=nullptr ? w->player() : nullptr))
      arpgSkillEnd(*pl);
    arpgSkillNow = SkNone;
    }
  arpgClick      = ArpgClick();
  arpgDodgeReq   = false;
  arpgPotionReq  = 0;
  arpgSpellSlot  = 0;
  arpgPushReq    = false;
  arpgBlocking   = false;
  arpgChargeHeld = false;
  if(arpgCharge!=ChargeNone) {
    auto w = Gothic::inst().world();
    if(auto pl = (w!=nullptr ? w->player() : nullptr))
      arpgChargeCancel(*pl);
    arpgCharge = ChargeNone;
    }
  }

void PlayerControl::arpgPush() {
  if(arpgPushCd>0.f)
    return;
  arpgPushReq  = true;
  arpgPushWait = 0.5f;
  }

bool PlayerControl::arpgCanPush() const {
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  if(pl==nullptr)
    return false;
  const auto ws = pl->weaponState();
  return ws==WeaponState::Bow || ws==WeaponState::CBow;
  }

// Gothic II: ItPo_Health_01..03, ItPo_Health_Addon_04, ItPo_Mana_..; Gothic: ItFo_Potion_Health_.., ItFo_Potion_Mana_..
// ItPo_Perm_* raise the maximum: never drunk by the keys. 1 = health, 2 = mana, 0 = other
static uint8_t arpgPotionKind(std::string_view n) {
  auto has = [n](std::string_view s) {
    if(s.size()>n.size())
      return false;
    for(size_t i=0; i+s.size()<=n.size(); ++i) {
      size_t k = 0;
      while(k<s.size() && std::toupper(uint8_t(n[i+k]))==uint8_t(s[k]))
        ++k;
      if(k==s.size())
        return true;
      }
    return false;
    };
  if(has("PERM") || !(has("ITPO_") || has("ITFO_POTION")))
    return 0;
  if(has("HEALTH"))
    return 1;
  if(has("MANA"))
    return 2;
  return 0;
  }

const Item* PlayerControl::arpgPickPotion(const Npc& pl, uint8_t kind, int32_t& total) const {
  // the smallest potion that is enough for what is missing, otherwise the biggest one
  total = 0;
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return nullptr;
  auto&         vm      = w->script().getVm();
  const auto    cur     = (kind==1) ? ATR_HITPOINTS    : ATR_MANA;
  const auto    max     = (kind==1) ? ATR_HITPOINTSMAX : ATR_MANAMAX;
  const int32_t missing = std::max(0, pl.attribute(max)-pl.attribute(cur));
  const Item*   best    = nullptr;
  int32_t       bestAmt = 0;
  bool          bestOk  = false;
  for(auto it = pl.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
    const Item& item = *it;
    if((ItmFlags(item.mainFlag()) & ITM_CAT_POTION)==0)
      continue;
    auto sym = vm.find_symbol_by_index(uint32_t(item.clsId()));
    if(sym==nullptr || arpgPotionKind(sym->name())!=kind)
      continue;
    total += int32_t(it.count());
    // Gothic II potions: TEXT[1]/COUNT[1] = how much it restores (COUNT[5] is the price)
    int32_t amt = item.handle().count[1];
    if(amt<=0) {
      for(size_t i=0; i<5; ++i)
        amt = std::max(amt, item.handle().count[i]);
      }
    if(amt<=0)
      amt = 1<<20; // full healing and such
    const bool ok = (amt>=missing);
    if(best==nullptr || (ok && (!bestOk || amt<bestAmt)) || (!ok && !bestOk && amt>bestAmt)) {
      best    = &item;
      bestAmt = amt;
      bestOk  = ok;
      }
    }
  return best;
  }

void PlayerControl::arpgPotion(uint8_t kind) {
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  if(pl==nullptr)
    return;
  int32_t total = 0;
  if(arpgPickPotion(*pl, kind, total)==nullptr) {
    Gothic::inst().onPrint(kind==1 ? "Brak mikstur zdrowia" : "Brak mikstur many");
    return;
    }
  const auto cur = (kind==1) ? ATR_HITPOINTS    : ATR_MANA;
  const auto max = (kind==1) ? ATR_HITPOINTSMAX : ATR_MANAMAX;
  if(pl->attribute(cur)>=pl->attribute(max)) {
    Gothic::inst().onPrint(kind==1 ? "Masz maksimum zdrowia" : "Masz maksimum many"); // no Polish letters: Gothic fonts
    return;
    }
  arpgPotionReq  = kind;
  arpgPotionWait = 1.5f;
  }

PlayerControl::ArpgBar PlayerControl::arpgBar() const {
  ArpgBar b;
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  if(pl==nullptr)
    return b;
  const auto ws = pl->weaponState();
  if(ws==WeaponState::Bow || ws==WeaponState::CBow)
    b.ranged = true;
  else if(ws==WeaponState::NoWeapon || ws==WeaponState::Mage)
    b.ranged = (wctrlLast==WeaponBow && arpgHasRanged(*pl));
  if(ws!=WeaponState::Fist)
    b.weapon = b.ranged ? pl->currentRangedWeapon() : pl->currentMeleeWeapon();
  for(uint8_t k=0; k<2; ++k)
    b.potion[k] = arpgPickPotion(*pl, uint8_t(k+1), b.potions[k]);
  b.charging = (arpgCharge!=ChargeNone);
  return b;
  }

// ---------------------------------------------------------------------------------------------------------------
// Diathic: the skills of the weapon trees: Q / E with the weapon in hand (Jednoręczna, Dwuręczna, Łuk i kusza),
// F = the tree Blok. A skill controls the hero while it goes on (PlayerControl::implMove -> arpgSkillRun).

namespace {
struct SkillInfo {
  const char* tree;
  const char* node;   // the node that unlocks it (its model on the bar)
  int32_t     popup;  // World::arpgPopup, kind 5
  };
}

static const SkillInfo& skillInfo(PlayerControl::ArpgSkill s) {
  static const SkillInfo infos[PlayerControl::SkCount] = {
    {"",            "",              0 },
    {"Blok",        "Uderzenie",     5 },
    {"Jednoręczna", "Pchnięcie",     7 },
    {"Jednoręczna", "Taniec ostrzy", 8 },
    {"Dwuręczna",   "Skok",          10},
    {"Dwuręczna",   "Młyn",          11},
    {"Łuk i kusza", "Deszcz strzał", 12},
    {"Łuk i kusza", "Odskok",        13},
    };
  return infos[s<PlayerControl::SkCount ? s : 0];
  }

static const char* skillIcon(PlayerControl::ArpgSkill s) {
  auto&        si = skillInfo(s);
  const size_t t  = ArpgSkills::treeOf(si.tree);
  if(t==size_t(-1))
    return nullptr;
  for(auto& b:ArpgSkills::trees()[t].branches)
    for(auto& n:b.nodes)
      if(std::string_view(n.name)==si.node)
        return n.icon;
  return nullptr;
  }

static float skillCooldown(PlayerControl::ArpgSkill s, const ArpgPerks& pk) {
  switch(s) {
    case PlayerControl::SkBash:      return std::max(1.f, ArpgStatus::BashCd-pk.bashCdCut);
    case PlayerControl::SkThrust:    return ArpgStatus::ThrustCd;
    case PlayerControl::SkDance:     return std::max(1.f, ArpgStatus::DanceCd-pk.danceCdCut);
    case PlayerControl::SkLeap:      return ArpgStatus::LeapCd;
    case PlayerControl::SkMill:      return ArpgStatus::MillCd;
    case PlayerControl::SkRain:      return ArpgStatus::RainCd;
    case PlayerControl::SkDisengage: return ArpgStatus::DisengageCd;
    case PlayerControl::SkNone:
    case PlayerControl::SkCount:
      break;
    }
  return 0.f;
  }

PlayerControl::ArpgSkill PlayerControl::arpgSkillFor(int slot, WeaponState ws) const {
  const bool melee  = (ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H);
  const bool ranged = (ws==WeaponState::Bow || ws==WeaponState::CBow);
  if(slot==2)
    return melee ? SkBash : SkNone;
  if(ws==WeaponState::W1H)
    return slot==0 ? SkThrust : SkDance;
  if(ws==WeaponState::W2H)
    return slot==0 ? SkLeap : SkMill;
  if(ranged)
    return slot==0 ? SkRain : SkDisengage;
  return SkNone;
  }

bool PlayerControl::arpgSkillKnown(ArpgSkill s) const {
  const auto& pk = heroPerks();
  switch(s) {
    case SkBash:      return pk.bash;
    case SkThrust:    return pk.thrust;
    case SkDance:     return pk.bladeDance;
    case SkLeap:      return pk.leap;
    case SkMill:      return pk.mill;
    case SkRain:      return pk.rain;
    case SkDisengage: return pk.disengage;
    case SkNone:
    case SkCount:
      break;
    }
  return false;
  }

PlayerControl::ArpgSkillSlot PlayerControl::arpgSkillSlot(int slot) const {
  ArpgSkillSlot r;
  auto w  = Gothic::inst().world();
  auto pl = w!=nullptr ? w->player() : nullptr;
  if(pl==nullptr)
    return r;
  auto ws = pl->weaponState();
  if(ws==WeaponState::NoWeapon || ws==WeaponState::Mage) {
    // no weapon in hand: what the key would draw
    if(slot!=2 && wctrlLast==WeaponBow && arpgHasRanged(*pl))
      ws = WeaponState::Bow;
    else if(auto m = pl->currentMeleeWeapon())
      ws = m->is2H() ? WeaponState::W2H : WeaponState::W1H;
    else
      ws = WeaponState::Fist;
    }
  r.skill = arpgSkillFor(slot, ws);
  if(r.skill==SkNone)
    return r;
  r.icon   = skillIcon(r.skill);
  r.known  = arpgSkillKnown(r.skill);
  r.active = (arpgSkillNow==r.skill) || (r.skill==SkRain && arpgRainLeft>0.f);
  r.total  = skillCooldown(r.skill, heroPerks());
  r.left   = r.total>0.f ? std::clamp(arpgSkillCd[r.skill]/r.total, 0.f, 1.f) : 0.f;
  return r;
  }

void PlayerControl::arpgSkillKey(int slot) {
  arpgSkillReq  = slot;
  arpgSkillWait = 0.5f; // pressed during a swing: as soon as the hero can
  arpgSkillDraw = false;
  }

bool PlayerControl::arpgSkillStart(Npc& pl, float& rot) {
  auto w = Gothic::inst().world();
  if(w==nullptr) {
    arpgSkillReq = -1;
    return false;
    }
  const int  slot = arpgSkillReq;
  const auto ws   = pl.weaponState();
  if(ws==WeaponState::NoWeapon || ws==WeaponState::Mage) {
    // the weapon first (F: the melee one), the skill goes off when it is in hand
    if(!arpgSkillDraw) {
      if(slot==2)
        wctrl[WeaponMele] = true; else
        arpgDrawPreferred(pl);
      arpgSkillDraw = true;
      arpgSkillWait = std::max(arpgSkillWait, 1.5f);
      }
    return false;
    }
  const ArpgSkill sk = arpgSkillFor(slot, ws);
  const auto&     pk = heroPerks();
  arpgSkillReq  = -1;
  arpgSkillDraw = false;
  if(sk==SkNone) {
    Gothic::inst().onPrint(ArpgMenu::gtxt(slot==2 ? "Uderzenie tylko z bronią białą albo pięściami"
                                                  : "Pięści nie mają umiejętności pod tym klawiszem"));
    return false;
    }
  if(!arpgSkillKnown(sk)) {
    auto& si = skillInfo(sk);
    Gothic::inst().onPrint(ArpgMenu::gtxt(std::string(si.node)+" - jeszcze nie znasz (drzewko "+si.tree+", "+
                                          ArpgKeys::keyName(ArpgKeys::key(ArpgKeys::Skills))+")"));
    return false;
    }
  if(arpgSkillCd[sk]>0.f)
    return false;
  const bool ranged = (ws==WeaponState::Bow || ws==WeaponState::CBow);
  if(ranged && !pl.hasAmmunition()) {
    Gothic::inst().onPrint(ArpgMenu::gtxt("Brak amunicji"));
    return false;
    }
  if(arpgCharge!=ChargeNone)
    arpgChargeCancel(pl);
  arpgClick    = ArpgClick();
  runAngleDest = 0;

  // where to: the enemy / the point under the cursor, otherwise straight ahead
  const auto    pos    = pl.position();
  Tempest::Vec3 aim    = pos;
  const bool    hasAim = arpgCursorAim(pl, aim);
  Tempest::Vec3 d      = aim-pos;
  d.y = 0;
  float dl = std::sqrt(d.x*d.x+d.z*d.z);
  if(!hasAim || dl<30.f) {
    const float rad = rot*float(M_PI)/180.f;
    d  = Tempest::Vec3(std::cos(rad), 0, std::sin(rad));
    dl = 0;
    } else {
    d /= dl;
    }
  float face = std::atan2(d.z, d.x)*180.f/float(M_PI);

  arpgSkillNow  = sk;
  arpgSkillT    = 0;
  arpgSkillLen  = 0;
  arpgSkillStep = 0;
  arpgSkillN    = 0;
  arpgSkillDist = 0;
  arpgSkillDir  = d;
  arpgSkillTg   = nullptr;
  arpgSkillHit.clear();
  arpgSkillCd[sk] = skillCooldown(sk, pk);
  pl.setAnimRotate(0);
  int32_t popup = skillInfo(sk).popup;

  switch(sk) {
    case SkBash: {
      // in the block pose (it blocks and parries on the way); Szarża: further, to the cursor
      arpgSkillDist  = pk.bashRun ? ArpgStatus::BashRunDist+pk.bashRunDist : ArpgStatus::BashDist;
      if(pk.bashRun && dl>0.f)
        arpgSkillDist = std::clamp(dl, ArpgStatus::BashDist, arpgSkillDist);
      arpgSkillSpeed = pk.bashRun ? ArpgStatus::BashRunSpeed : ArpgStatus::BashSpeed;
      if(pk.bashRun)
        popup = 6;
      pl.stopAnim("");
      rot = face;
      pl.setDirection(rot);
      if(ws==WeaponState::Fist)
        pl.blockFist(); else
        pl.blockSword();
      pl.arpgHoldParade();
      break;
      }
    case SkThrust: {
      arpgSkillDist  = ArpgStatus::ThrustDist+pk.thrustDist;
      if(dl>0.f)
        arpgSkillDist = std::min(arpgSkillDist, std::max(dl, 150.f));
      arpgSkillSpeed = ArpgStatus::ThrustSpeed;
      pl.stopAnim("");
      rot = face;
      pl.setDirection(rot);
      pl.swingSword();
      pl.arpgNoCommit(w->tickCount()+1500); // the skill deals the damage of this swing
      break;
      }
    case SkDance: {
      pl.arpgSetSkillSpeed(ArpgStatus::DanceSpeed);
      break;
      }
    case SkLeap: {
      arpgSkillDist  = ArpgStatus::LeapDist+pk.leapDist;
      if(dl>0.f)
        arpgSkillDist = std::min(arpgSkillDist, dl);
      arpgSkillSpeed = ArpgStatus::LeapSpeed;
      pl.stopAnim("");
      rot = face;
      pl.setDirection(rot);
      pl.swingSword();
      pl.arpgNoCommit(w->tickCount()+2000);
      break;
      }
    case SkMill: {
      arpgSkillLen  = ArpgStatus::MillTime+pk.millTime;
      arpgSkillStep = 0.15f; // the first hit soon
      pl.stopAnim("");
      rot = face;
      pl.setDirection(rot);
      if(!pl.swingSwordL())
        pl.swingSwordR();
      pl.arpgHoldAttack();
      pl.arpgSetMill(w->tickCount()+300);
      break;
      }
    case SkRain: {
      // the hero shoots up for a moment; the rain goes on by itself (arpgTickRain)
      arpgRainAt   = hasAim ? aim : pos+d*600.f;
      arpgRainLeft = ArpgStatus::RainTime+pk.rainTime;
      arpgRainNext = 0.3f;
      arpgRainR    = ArpgStatus::RainRadius+pk.rainRadius;
      arpgSkillLen = 0.4f;
      rot = face;
      pl.setDirection(rot);
      pl.aimBow();
      Npc::ArpgFx fx;
      fx.time  = w->tickCount();
      fx.pos   = arpgRainAt;
      fx.arc   = 360.f;
      fx.reach = arpgRainR;
      fx.kind  = 3;
      pl.arpgSetFx(fx);
      break;
      }
    case SkDisengage: {
      // the target: under the cursor or the nearest enemy; the jump: away from it (or from the cursor)
      Npc* tg = nullptr;
      if(arpgHover.npc!=nullptr && arpgIsEnemy(*arpgHover.npc))
        tg = arpgHover.npc; else
        tg = arpgNextEnemy(pl, nullptr, arpgShotRange());
      Tempest::Vec3 away = Tempest::Vec3(-d.x, 0, -d.z);
      if(tg!=nullptr) {
        auto        t = tg->position()-pos;
        const float l = std::sqrt(t.x*t.x+t.z*t.z);
        if(l>1.f) {
          away = Tempest::Vec3(-t.x/l, 0, -t.z/l);
          face = std::atan2(t.z, t.x)*180.f/float(M_PI);
          }
        }
      arpgSkillTg    = tg;
      arpgSkillDir   = away;
      arpgSkillDist  = ArpgStatus::DisengageDist+pk.disengageDist;
      arpgSkillSpeed = ArpgStatus::DisengageSpeed;
      pl.stopAnim("");
      rot = face;
      pl.setDirection(rot);
      pl.setAnimAngGet(Npc::Anim::MoveBack); // the jump back, when the weapon has one
      break;
      }
    case SkNone:
    case SkCount:
      break;
    }
  if(popup!=0)
    w->arpgPopup(pos+Tempest::Vec3(0,230.f,0), popup, 5);
  Tempest::Log::i("Diathic: skill ", skillInfo(sk).node);
  return true;
  }

bool PlayerControl::arpgSkillRun(Npc& pl, float& rot, float dtF) {
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return false;
  const auto ws = pl.weaponState();

  // Riposta: a parry - the counter swing at once (the block waits for it)
  if(Npc* rt = pl.arpgTakeRiposte()) {
    const bool melee = (ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H);
    if(melee && arpgSkillNow==SkNone && !rt->isDown()) {
      pl.stopAnim("");
      const auto t = rt->position()-pl.position();
      if(t.x*t.x+t.z*t.z>1.f) {
        rot = std::atan2(t.z, t.x)*180.f/float(M_PI);
        pl.setDirection(rot);
        }
      setTarget(rt);
      if(ws==WeaponState::Fist ? pl.fistShoot() : pl.swingSword()) {
        arpgRiposteT = ArpgStatus::RiposteTime;
        w->arpgPopup(pl.position()+Tempest::Vec3(0,230.f,0), 14, 5);
        }
      }
    }
  if(arpgRiposteT>0.f) {
    arpgRiposteT -= dtF;
    runAngleDest  = 0;
    return true;
    }

  if(arpgSkillNow==SkNone)
    return false;
  arpgSkillT  += dtF;
  runAngleDest = 0;
  const auto& pk = heroPerks();

  // a dash: the hero is moved along arpgSkillDir; false: done (the distance is gone or the way is blocked)
  auto dash = [&]() -> bool {
    if(arpgSkillDist<=0.f)
      return false;
    const float step = std::min(arpgSkillDist, arpgSkillSpeed*dtF);
    arpgSkillDist -= step;
    if(!pl.tryMove(arpgSkillDir*step)) {
      arpgSkillDist = 0;
      return false;
      }
    return arpgSkillDist>0.f;
    };
  // enemies around a point (flat distance, up to 2 m up or down); cosHalf: only in front of the dash (-2: all round)
  auto enemies = [&](const Tempest::Vec3& at, float r, float cosHalf, auto fn) {
    w->detectNpc(at, r+100.f, [&](Npc& n) {
      if(&n==&pl || n.isDown() || !arpgIsEnemy(n))
        return;
      const auto  dd = n.position()-at;
      const float l2 = dd.x*dd.x+dd.z*dd.z;
      if(l2>r*r || std::abs(dd.y)>200.f)
        return;
      if(cosHalf>-1.5f && l2>60.f*60.f) {
        const float l = std::sqrt(l2);
        if((dd.x*arpgSkillDir.x+dd.z*arpgSkillDir.z)/l<cosHalf)
          return;
        }
      fn(n);
      });
    };
  auto once = [&](Npc& n) {
    if(std::find(arpgSkillHit.begin(), arpgSkillHit.end(), &n)!=arpgSkillHit.end())
      return false;
    arpgSkillHit.push_back(&n);
    return true;
    };
  auto ring = [&](const Tempest::Vec3& at, float r) {
    Npc::ArpgFx fx;
    fx.time  = w->tickCount();
    fx.pos   = at;
    fx.ang   = pl.rotation();
    fx.arc   = 360.f;
    fx.reach = r;
    fx.kind  = 3;
    pl.arpgSetFx(fx);
    };

  switch(arpgSkillNow) {
    case SkBash: {
      pl.arpgHoldParade(); // the block held on the way
      const bool going = dash();
      const auto pos   = pl.position();
      if(pk.bashRun) {
        // Szarża: everyone on the way falls
        Npc::ArpgSkillHit h;
        h.mul   = ArpgStatus::BashMul;
        h.knock = true;
        enemies(pos, 120.f, -2.f, [&](Npc& n) { if(once(n)) pl.arpgSkillDamage(n, h); });
        }
      Npc* front = nullptr;
      enemies(pos, 130.f, 0.5f, [&](Npc& n) {
        if(std::find(arpgSkillHit.begin(), arpgSkillHit.end(), &n)==arpgSkillHit.end())
          front = &n;
        });
      if(going && front==nullptr)
        return true;
      // the bash: the enemies in front staggered
      Npc::ArpgSkillHit h;
      h.mul     = ArpgStatus::BashMul+pk.bashDmg;
      h.stagger = ArpgStatus::BashStaggerMs+uint64_t((pk.bashStagger+pk.staggerAdd)*1000.f);
      enemies(pos, 170.f, 0.5f, [&](Npc& n) { if(once(n)) pl.arpgSkillDamage(n, h); });
      pl.emitSoundEffect("WHOOSH", 1500.f, true);
      arpgSkillEnd(pl);
      return false;
      }
    case SkThrust: {
      const bool going = dash();
      Npc::ArpgSkillHit h;
      h.mul   = ArpgStatus::ThrustMul+pk.thrustDmg;
      h.bleed = ArpgStatus::ThrustBleed;
      enemies(pl.position(), 160.f, 0.3f, [&](Npc& n) { if(once(n)) pl.arpgSkillDamage(n, h); });
      if(going)
        return true;
      arpgSkillEnd(pl);
      return false;
      }
    case SkDance: {
      if(ws!=WeaponState::W1H) {
        arpgSkillEnd(pl);
        return false;
        }
      arpgSkillStep += dtF;
      const bool hitDone = pl.arpgCommits()>arpgSkillCommits;
      if(arpgSkillN>0 && !hitDone && arpgSkillStep<ArpgStatus::DanceCutMax)
        return true; // the cut goes on
      if(arpgSkillN>=pk.danceHits) {
        arpgSkillEnd(pl);
        return false;
        }
      // the next cut, at the cursor (the enemy under it, the nearest close one)
      const float face = arpgFightFace(pl, rot);
      pl.stopAnim("");
      rot = face;
      pl.setDirection(rot);
      bool ok = (arpgSkillN%2==0) ? pl.swingSwordR() : pl.swingSwordL();
      if(!ok)
        ok = pl.swingSword();
      arpgSkillCommits = pl.arpgCommits();
      arpgSkillStep    = 0;
      arpgSkillN++;
      return true;
      }
    case SkLeap: {
      if(dash())
        return true;
      // the landing: everyone around falls
      const auto pos = pl.position();
      Npc::ArpgSkillHit h;
      h.mul   = ArpgStatus::LeapMul+pk.leapDmg;
      h.knock = true;
      enemies(pos, ArpgStatus::LeapRadius, -2.f, [&](Npc& n) { pl.arpgSkillDamage(n, h); });
      ring(pos, ArpgStatus::LeapRadius);
      pl.emitSoundEffect("WHOOSH", 1500.f, true);
      arpgSkillEnd(pl);
      return false;
      }
    case SkMill: {
      const bool held = arpgHeld[ArpgKeys::SkillB];
      if((!held && arpgSkillT>0.4f) || arpgSkillT>=arpgSkillLen || ws!=WeaponState::W2H) {
        arpgSkillEnd(pl);
        return false;
        }
      pl.arpgHoldAttack();
      pl.arpgSetMill(w->tickCount()+300);
      rot += ArpgStatus::MillTurn*dtF;
      pl.setDirection(rot);
      // slowly towards the cursor
      Tempest::Vec3 aim;
      if(arpgCursorAim(pl, aim)) {
        auto        t = aim-pl.position();
        const float l = std::sqrt(t.x*t.x+t.z*t.z);
        if(l>50.f)
          pl.tryMove(Tempest::Vec3(t.x/l, 0, t.z/l)*std::min(l, ArpgStatus::MillMove*dtF));
        }
      arpgSkillStep -= dtF;
      if(arpgSkillStep<=0.f) {
        arpgSkillStep += ArpgStatus::MillTick;
        const auto pos = pl.position();
        Npc::ArpgSkillHit h;
        h.mul = ArpgStatus::MillMul+pk.millDmg;
        enemies(pos, ArpgStatus::MillRadius, -2.f, [&](Npc& n) { pl.arpgSkillDamage(n, h); });
        ring(pos, ArpgStatus::MillRadius);
        }
      return true;
      }
    case SkRain: {
      if(arpgSkillT<arpgSkillLen) {
        pl.aimBow();
        return true;
        }
      arpgSkillEnd(pl);
      return false;
      }
    case SkDisengage: {
      auto shot = [&]() {
        const Npc* tg = arpgSkillTg;
        if(tg!=nullptr && tg->isDown())
          tg = nullptr;
        if(tg!=nullptr) {
          const auto t = tg->position()-pl.position();
          if(t.x*t.x+t.z*t.z>1.f) {
            rot = std::atan2(t.z, t.x)*180.f/float(M_PI);
            pl.setDirection(rot);
            }
          }
        const float   rad = rot*float(M_PI)/180.f;
        Tempest::Vec3 vel = Tempest::Vec3(std::cos(rad), 0, std::sin(rad))*DynamicWorld::bulletSpeed;
        if(pl.arpgSkillArrow(nullptr, vel, tg, ArpgStatus::DisengageMul))
          pl.arpgUseArrow();
        };
      if(arpgSkillN==0) {
        if(dash())
          return true;
        shot();
        arpgSkillN    = 1;
        arpgSkillStep = 0;
        if(pk.doubleShot && pl.hasAmmunition())
          return true;
        arpgSkillEnd(pl);
        return false;
        }
      arpgSkillStep += dtF;
      if(arpgSkillStep<0.15f)
        return true;
      shot(); // Podwójny strzał
      arpgSkillEnd(pl);
      return false;
      }
    case SkNone:
    case SkCount:
      break;
    }
  arpgSkillEnd(pl);
  return false;
  }

void PlayerControl::arpgSkillEnd(Npc& pl) {
  switch(arpgSkillNow) {
    case SkDance:
      pl.arpgSetSkillSpeed(0);
      break;
    case SkMill:
      pl.arpgSetMill(0);
      pl.stopAnim(""); // drop the held swing
      break;
    case SkBash:
      if(!arpgBlocking)
        pl.stopAnim(""); // the block pose (held on: the block goes on)
      break;
    default:
      break;
    }
  arpgSkillNow  = SkNone;
  arpgSkillDist = 0;
  arpgSkillTg   = nullptr;
  arpgSkillHit.clear();
  }

void PlayerControl::arpgTickRain(Npc& pl, float dtF) {
  // Deszcz strzał: arrows from the sky on random points of the circle; the volley uses one arrow at the end
  if(arpgRainLeft<=0.f)
    return;
  auto w = Gothic::inst().world();
  if(w==nullptr) {
    arpgRainLeft = 0;
    return;
    }
  arpgRainLeft -= dtF;
  arpgRainNext -= dtF;
  while(arpgRainNext<=0.f && arpgRainLeft>0.f) {
    arpgRainNext += ArpgStatus::RainEvery;
    const float a  = float(w->script().rand(3600))*0.1f*float(M_PI)/180.f;
    const float r  = arpgRainR*std::sqrt(float(w->script().rand(1000))/1000.f);
    const auto  at = arpgRainAt+Tempest::Vec3(std::cos(a)*r, ArpgStatus::RainHeight, std::sin(a)*r);
    const auto  v  = Tempest::Vec3(0.f, -DynamicWorld::bulletSpeed*0.8f, 0.f);
    if(!pl.arpgSkillArrow(&at, v, nullptr, ArpgStatus::RainMul)) {
      arpgRainLeft = 0; // the bow went away
      break;
      }
    }
  if(arpgRainLeft<=0.f) {
    arpgRainLeft = 0;
    pl.arpgUseArrow();
    }
  }
