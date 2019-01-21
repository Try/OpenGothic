#include "menuroot.h"

#include "ui/gamemenu.h"
#include "gothic.h"

using namespace Tempest;

MenuRoot::MenuRoot(Gothic &gothic)
  :gothic(gothic){
  setFocus(true);
  }

MenuRoot::~MenuRoot() {
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
    if(gothic.isInGame()) {
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

void MenuRoot::mouseDownEvent(MouseEvent &event) {
  if(current==nullptr)
    event.ignore();
  }

void MenuRoot::mouseUpEvent(MouseEvent&) {
  if(current!=nullptr)
    current->onSelect();
  }

void MenuRoot::mouseWheelEvent(MouseEvent &event) {
  if(current!=nullptr)
    current->onMove(-event.delta/120); else
    event.ignore();
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
    if(e.key==Event::K_Return)
      current->onSelect();
    if(e.key==Event::K_ESCAPE)
      popMenu();
    }
  }
