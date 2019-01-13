#include "menuroot.h"

#include "ui/gamemenu.h"
#include "gothic.h"

MenuRoot::MenuRoot(Gothic &gothic)
  :gothic(gothic){
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
      }
    return;
    }
  GameMenu* w = menuStack.back().release();

  removeAllWidgets();
  addWidget(w);
  current=w;

  menuStack.pop_back();
  }

void MenuRoot::mouseDownEvent(Tempest::MouseEvent &event) {
  event.accept();
  }

void MenuRoot::mouseUpEvent(Tempest::MouseEvent&) {
  if(current!=nullptr)
    current->onSelect();
  }

void MenuRoot::mouseWheelEvent(Tempest::MouseEvent &event) {
  if(current!=nullptr)
    current->onMove(-event.delta/120);
  }
