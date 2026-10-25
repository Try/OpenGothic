#include "menuroot.h"

#include <Tempest/Log>

#include "ui/gamemenu.h"
#include "ui/gamepadsettingsmenu.h"
#include "gothic.h"

using namespace Tempest;

MenuRoot::MenuRoot(KeyCodec& keyCodec)
  :keyCodec(keyCodec) {
  setCursorShape(CursorShape::Hidden);

  Gothic::inst().onSettingsChanged.bind(this,&MenuRoot::initSettings);
  initSettings();
  }

MenuRoot::~MenuRoot() {
  Gothic::inst().onSettingsChanged.ubind(this,&MenuRoot::initSettings);
  }

void MenuRoot::initSettings() {
  const auto lang = Gothic::inst().settingsGetI("GAME", "language");
  if(lang==vmLang && vm!=nullptr)
    return;

  // clear pointer to vm-objects
  for(auto& i:menuStack)
    i->resetVm(nullptr);
  if(current!=nullptr)
    current->resetVm(nullptr);

  vmLang = lang;
  vm     = Gothic::inst().createPhoenixVm("MENU.DAT", ScriptLang(vmLang));
  vm->register_external("apply_options_video",[](){
    Log::d("TODO: apply_options_video");
    });

  for(auto& i:menuStack)
    i->resetVm(vm.get());
  if(current!=nullptr)
    current->resetVm(vm.get());

  update();
  }

MenuRoot::ModalInputScope::ModalInputScope(MenuRoot& root, MenuModalInput::Handler handler)
  :ModalInputScope(static_cast<Widget&>(root),root.modalInput,std::move(handler)) {
  }

MenuRoot::ModalInputScope::ModalInputScope(Widget& repaintRoot, MenuModalInput& input, MenuModalInput::Handler handler)
  :scope(input,std::move(handler)) {
  // A new overlay is unbound: invalidate the real window before nested exec.
  repaintRoot.update();
  }

MenuRoot::ModalInputScope::~ModalInputScope() = default;

void MenuRoot::onGamepad(KeyCodec::Action action) {
  // Modal exec suspends the menu timer, so invalidate before entering its loop.
  update();
  if(hasModalInput()) {
    modalInput.dispatch(action);
    return;
    }
  if(current==nullptr)
    return;
  if(action==KeyCodec::UiSettings) {
    showGamepadSettings();
    return;
    }
  if(action==KeyCodec::Escape) {
    popMenu();
    return;
    }
  if(action==KeyCodec::Forward || action==KeyCodec::Back ||
     action==KeyCodec::Left || action==KeyCodec::Right ||
     action==KeyCodec::ActionGeneric)
    current->onGamepad(action);
  }

void MenuRoot::showGamepadSettings() {
  if(current==nullptr || hasModalInput())
    return;
  GamepadSettingsMenu dialog(GamepadSettings::parse(Gothic::settingsGetS("GAMEPAD","profile")),
                             Gothic::settingsGetI("GAME","language")==int(ScriptLang::RU),
                             Gothic::interfaceScale(this));
  dialog.resize(size());
  {
  ModalInputScope modal(*this,[&dialog](KeyCodec::Action action) { dialog.onGamepad(action); });
  dialog.exec();
  }
  // The dialog is closed and its decision final before a settings signal can
  // reenter UI or invalidate the previous controller bindings.
  if(dialog.accepted()) {
    Gothic::settingsSetS("GAMEPAD","profile",dialog.settings().serialize());
    Gothic::flushSettings();
    }
  update();
  }

void MenuRoot::setMainMenu() {
  setMenu(Gothic::inst().menuMain());
  showVersion(true);
  }

void MenuRoot::setMenu(std::string_view menuEv, KeyCodec::Action key) {
  if(vm->find_symbol_by_name(menuEv) == nullptr){
    Log::e("invalid menu-id: ",menuEv);
    return;
    }
  setMenu(new GameMenu(*this,keyCodec,*vm,menuEv,key));
  }

void MenuRoot::setMenu(GameMenu *w) {
  ++inputRevisionCounter;
  removeAllWidgets();
  if(w)
    addWidget(w);
  current=w;
  }

void MenuRoot::pushMenu(GameMenu *w) {
  ++inputRevisionCounter;
  if(widgetsCount()>0) {
    takeWidget(current);
    menuStack.emplace_back(current);
    }
  if(w) {
    addWidget(w);
    w->onTick();
    }
  current = w;
  }

void MenuRoot::popMenu() {
  if(menuStack.size()==0) {
    if(Gothic::inst().isInGame() || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle) {
      ++inputRevisionCounter;
      current=nullptr;
      removeAllWidgets();
      owner()->setFocus(true);
      }
    return;
    }
  ++inputRevisionCounter;
  GameMenu* w = menuStack.back().release();

  removeAllWidgets();
  addWidget(w);
  current=w;
  current->onTick();

  menuStack.pop_back();
  }

void MenuRoot::closeAll() {
  ++inputRevisionCounter;
  menuStack.clear();
  if(Gothic::inst().isInGame() || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle) {
    current=nullptr;
    removeAllWidgets();
    }
  }

bool MenuRoot::isActive() const {
  return current!=nullptr;
  }

void MenuRoot::setPlayer(const Npc &pl) {
  if(current!=nullptr)
    current->setPlayer(pl);
  }

void MenuRoot::processMusicTheme() {
  if(current!=nullptr)
    current->processMusicTheme();
  }

void MenuRoot::showVersion(bool s) {
  showVersionHint = s;
  update();
  }

bool MenuRoot::hasVersionLine() const {
  return showVersionHint;
  }

void MenuRoot::mouseDownEvent(MouseEvent& event) {
  // Only the first pointer interacts; ignoring others keeps their drag and up events away.
  if(current==nullptr || event.mouseID!=0) {
    event.ignore();
    return;
    }
  if(event.button==Event::ButtonRight) {
    popMenu();
    return;
    }
#if defined(__MOBILE_PLATFORM__)
  touchStart = event.pos();
  touchMoved = false;
  current->onBeginTouch(touchStart-current->pos());
#else
  current->onKeyboard(KeyCodec::ActionGeneric);
#endif
  }

void MenuRoot::mouseDragEvent(MouseEvent& event) {
#if defined(__MOBILE_PLATFORM__)
  if(current==nullptr)
    return;
  const int step = std::max(1, int(20.f*Gothic::interfaceScale(this)));
  const auto delta = event.pos()-touchStart;
  touchMoved |= std::max(std::abs(delta.x),std::abs(delta.y))>=step;
  current->onDragTouch(event.pos()-current->pos());
#else
  (void)event;
#endif
  }

void MenuRoot::mouseUpEvent(MouseEvent& event) {
#if defined(__MOBILE_PLATFORM__)
  if(current==nullptr)
    return;
  mouseDragEvent(event);
  current->onEndTouch(!touchMoved);
#else
  (void)event;
#endif
  }

void MenuRoot::mouseWheelEvent(MouseEvent &event) {
  if(current!=nullptr) {
    if(event.delta>0)
      current->onKeyboard(KeyCodec::Forward); else
    if(event.delta<0)
      current->onKeyboard(KeyCodec::Back);
    } else {
    event.ignore();
    }
  }

void MenuRoot::keyRepeatEvent(Tempest::KeyEvent &e) {
  if(current==nullptr) {
    e.ignore();
    return;
    }

  if(e.key == Event::K_A || e.key == Event::K_Left)
    current->onKeyboard(KeyCodec::Left);
  else if(e.key == Event::K_D || e.key == Event::K_Right)
    current->onKeyboard(KeyCodec::Right);
  else if(e.key == Event::K_W || e.key == Event::K_Up)
    current->onKeyboard(KeyCodec::Forward);
  else if(e.key == Event::K_S || e.key == Event::K_Down)
    current->onKeyboard(KeyCodec::Back);
  }

void MenuRoot::keyDownEvent(KeyEvent &e) {
  if(e.key==Event::K_F1 && current!=nullptr && !hasModalInput()) {
    e.accept();
    showGamepadSettings();
    return;
    }
  const float scale = Gothic::interfaceScale(this);
  size_t sz = std::extent_v<decltype(cheatCode)>;
  for(size_t i=1; i<sz; ++i)
    cheatCode[i-1] = cheatCode[i];
  cheatCode[sz-1] = e.key;
  if(cheatCode[0]==Event::K_M &&
     cheatCode[1]==Event::K_A &&
     cheatCode[2]==Event::K_R &&
     cheatCode[3]==Event::K_V &&
     cheatCode[4]==Event::K_I &&
     cheatCode[5]==Event::K_N) {
    Gothic::inst().setMarvinEnabled(true);
    auto& fnt = Resources::font(scale);
    Gothic::inst().onPrintScreen("MARVIN-MODE",2,4, 1,fnt);
    }

  if(cheatCode[sz-2]==Event::K_4 &&
     cheatCode[sz-1]==Event::K_2) {
    Gothic::inst().setMarvinEnabled(false);
    auto& fnt = Resources::font(scale);
    Gothic::inst().onPrintScreen("WHAT WAS THE QUESTION?",2,4, 1,fnt);
    }

  if(current!=nullptr) {
    if(e.key==Event::K_W || e.key==Event::K_Up)
      current->onKeyboard(KeyCodec::Forward);
    else if(e.key==Event::K_S || e.key==Event::K_Down)
      current->onKeyboard(KeyCodec::Back);
    else if(e.key==Event::K_A || e.key==Event::K_Left)
      current->onKeyboard(KeyCodec::Left);
    else if(e.key==Event::K_D || e.key==Event::K_Right)
      current->onKeyboard(KeyCodec::Right);
    else if(e.key==Event::K_Return)
      current->onKeyboard(KeyCodec::ActionGeneric);
    else if(e.key==Event::K_Delete)
      current->onKeyboard(KeyCodec::K_Del);
    else if(e.key==Event::K_ESCAPE || keyCodec.tr(e)==current->keyClose())
      popMenu();
    }
  }
