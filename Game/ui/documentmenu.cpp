#include "documentmenu.h"

#include "gothic.h"

using namespace Tempest;

DocumentMenu::DocumentMenu(Gothic& gothic)
  :gothic(gothic) {
  }

void DocumentMenu::show(const DocumentMenu::Show &doc) {
  document = doc;
  active   = true;
  update();
  }

void DocumentMenu::close() {
  if(!active)
    return;
  active=false;
  }

void DocumentMenu::keyDownEvent(KeyEvent &e) {
  if(!active){
    e.ignore();
    return;
    }

  if(e.key!=Event::K_ESCAPE){
    e.ignore();
    return;
    }
  close();
  }

void DocumentMenu::keyUpEvent(KeyEvent&) {
  }

void DocumentMenu::paintEvent(PaintEvent &e) {
  if(!active)
    return;

  int x=0;
  Painter p(e);
  p.setFont(Resources::dialogFont());

  for(auto& i:document.pages){
    auto back = Resources::loadTexture((i.flg&F_Backgr) ? i.img : document.img);
    if(!back)
      continue;
    auto& mgr = (i.flg&F_Margin) ? i.margins : document.margins;

    const int w = (800+mgr.xMargin())/document.pages.size();

    p.setBrush(*back);
    p.drawRect(x,0,w,back->h(),
               0,0,back->w(),back->h());

    p.setBrush(Color(0.04f,0.04f,0.04f,1));

    p.drawText(x+mgr.left,
               mgr.top,
               w - mgr.xMargin(),
               back->h() - mgr.yMargin(),
               i.text);
    x+=w;
    }
  }
