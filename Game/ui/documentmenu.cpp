#include "documentmenu.h"

#include "utils/gthfont.h"
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

  if(auto pl = gothic.player())
    pl->setInteraction(nullptr);
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

  float mw = 0, mh = 0;
  for(auto& i:document.pages){
    auto back = Resources::loadTexture((i.flg&F_Backgr) ? i.img : document.img);
    if(!back)
      continue;
    mw += float(back->w());
    mh = std::max(mh,float(back->h()));
    }

  float k = std::min(1.f,float(800+document.margins.xMargin())/std::max(mw,1.f));

  int x=(w()-int(k*mw))/2, y = (h()-int(mh))/2;

  Painter p(e);
  for(auto& i:document.pages){
    const GthFont* fnt = nullptr;
    if(i.flg&F_Font)
      fnt = &Resources::font(i.font.c_str()); else
      fnt = &Resources::font(document.font.c_str());
    if(fnt==nullptr)
      fnt = &Resources::font();
    auto back = Resources::loadTexture((i.flg&F_Backgr) ? i.img : document.img);
    if(!back)
      continue;
    auto& mgr = (i.flg&F_Margin) ? i.margins : document.margins;

    const int w = int(k*float(back->w()));

    p.setBrush(*back);
    p.drawRect(x,y,w,back->h(),
               0,0,back->w(),back->h());

    p.setBrush(Color(0.04f,0.04f,0.04f,1));

    fnt->drawText(p,x+mgr.left,
                  y+mgr.top,
                  w - mgr.xMargin(),
                  back->h() - mgr.yMargin(),
                  i.text, Tempest::AlignLeft);
    x+=w;
    }
  }
