#include "menuroot.h"

#include <Tempest/Log>

#include "ui/gamemenu.h"
#include "gothic.h"

using namespace Tempest;

MenuRoot::MenuRoot(Gothic &gothic)
  :gothic(gothic){
  vm = gothic.createVm(u"MENU.DAT");
  vm->registerUnsatisfiedLink([](Daedalus::DaedalusVM&){});

  vm->registerExternalFunction("playvideo",   [this](Daedalus::DaedalusVM& vm){ playvideo(vm);   });
  vm->registerExternalFunction("playvideoex", [this](Daedalus::DaedalusVM& vm){ playvideoex(vm); });
  }

MenuRoot::~MenuRoot() {
  }

void MenuRoot::setMenu(const char *menuEv) {
  if(!vm->getDATFile().hasSymbolName(menuEv)){
    Log::e("invalid menu-id: ",menuEv);
    return;
    }
  setMenu(new GameMenu(*this,*vm,gothic,menuEv));
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
  current=w;
  }

void MenuRoot::popMenu() {
  if(menuStack.size()==0) {
    if(gothic.isInGame() || gothic.checkLoading()!=Gothic::LoadState::Idle) {
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
  if(gothic.isInGame() || gothic.checkLoading()!=Gothic::LoadState::Idle) {
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

void MenuRoot::mouseDownEvent(MouseEvent& event) {
  if(current!=nullptr)
    current->onSelect(); else
    event.ignore();
  }

void MenuRoot::mouseUpEvent(MouseEvent&) {
  }

void MenuRoot::mouseWheelEvent(MouseEvent &event) {
  if(current!=nullptr) {
    if(event.delta>0)
      current->onMove(-1); else
    if(event.delta<0)
      current->onMove(1);
    } else {
    event.ignore();
    }
  }

void MenuRoot::keyDownEvent(KeyEvent &e) {
  e.accept();
  }

void MenuRoot::keyUpEvent(KeyEvent &e) {
  if(current!=nullptr){
    if(e.key==Event::K_W || e.key==Event::K_Up)
      current->onMove(-1);
    if(e.key==Event::K_S || e.key==Event::K_Down)
      current->onMove(1);
    if(e.key==Event::K_A || e.key==Event::K_Left)
      current->onSlide(-1);
    if(e.key==Event::K_D || e.key==Event::K_Right)
      current->onSlide(1);
    if(e.key==Event::K_Return)
      current->onSelect();
    if(e.key==Event::K_ESCAPE)
      popMenu();
    }
  }

void MenuRoot::playvideo(Daedalus::DaedalusVM& vm) {
  Daedalus::ZString filename = vm.popString();
  gothic.playVideo(filename);
  vm.setReturn(0);
  }

void MenuRoot::playvideoex(Daedalus::DaedalusVM& vm) {
  int exitSession = vm.popInt();
  int screenBlend = vm.popInt();

  (void)exitSession; // TODO: ex-fetures
  (void)screenBlend;

  Daedalus::ZString filename = vm.popString();
  gothic.playVideo(filename);
  vm.setReturn(0);
  }
