#include "menuroot.h"

#include <Tempest/Log>

#include "ui/gamemenu.h"
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

void MenuRoot::setMainMenu() {
  setMenu("MENU_MAIN");
  showVersion(true);
  }

void MenuRoot::setMenu(const char* menuEv, KeyCodec::Action key) {
  if(vm->find_symbol_by_name(menuEv) == nullptr){
    Log::e("invalid menu-id: ",menuEv);
    return;
    }
  setMenu(new GameMenu(*this,keyCodec,*vm,menuEv,key));
  }

void MenuRoot::setMenu(GameMenu *w) {
  removeAllWidgets();
  if(w)
    addWidget(w);
  current=w;
  }

void MenuRoot::pushMenu(GameMenu *w) {
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
      current=nullptr;
      removeAllWidgets();
      owner()->setFocus(true);
      }
    return;
    }
  GameMenu* w = menuStack.back().release();

  removeAllWidgets();
  addWidget(w);
  current=w;

  menuStack.pop_back();
  }

void MenuRoot::closeAll() {
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
  if(current!=nullptr) {
    if(event.button==Event::ButtonRight) {
      popMenu();
      } else {
      current->onKeyboard(KeyCodec::ActionGeneric);
      }
    } else {
    event.ignore();
    }
  }

void MenuRoot::mouseUpEvent(MouseEvent&) {
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

void MenuRoot::keyDownEvent(KeyEvent &e) {
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
    auto& fnt = Resources::font();
    Gothic::inst().onPrintScreen("MARVIN-MODE",2,4, 1,fnt);
    }

  if(cheatCode[sz-2]==Event::K_4 &&
     cheatCode[sz-1]==Event::K_2) {
    Gothic::inst().setMarvinEnabled(false);
    auto& fnt = Resources::font();
    Gothic::inst().onPrintScreen("WHAT WAS THE QUESTION?",2,4, 1,fnt);
    }
  }

void MenuRoot::keyUpEvent(KeyEvent &e) {
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
    else if(e.key==Event::K_ESCAPE || keyCodec.tr(e)==current->keyClose())
      popMenu();
    }
  }
