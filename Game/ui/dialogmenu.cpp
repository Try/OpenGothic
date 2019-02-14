#include "dialogmenu.h"

#include <Tempest/Painter>

#include "resources.h"

using namespace Tempest;

DialogMenu::DialogMenu() {
  tex = Resources::loadTexture("DLG_CHOICE.TGA");
  }

void DialogMenu::tick(uint64_t dt) {
  if(txt.size()==0)
    return;

  if(remDlg<dt) {
    txt.pop_back();
    if(txt.size()>0) {
      remDlg=txt.back().time;
      onEntry(txt.back());
      } else {
      remDlg=0;
      }
    } else {
    remDlg-=dt;
    }

  for(size_t i=0;i<pscreen.size();){
    auto& p=pscreen[i];
    if(p.time<dt){
      pscreen.erase(pscreen.begin()+int(i));
      } else {
      p.time-=dt;
      ++i;
      }
    }
  update();
  }

void DialogMenu::aiOutput(const char *msg) {
  Entry e;
  e.txt=msg;
  e.time = uint32_t(2000);
  if(txt.size()==0)
    remDlg=e.time;
  txt.emplace(txt.begin(),std::move(e));
  update();
  }

void DialogMenu::aiClose() {
  Entry e;
  e.flag = DlgClose;
  txt.emplace(txt.begin(),std::move(e));
  update();
  }

void DialogMenu::printScreen(const char *msg, int x, int y, int time, const Tempest::Font &font) {
  PScreen e;
  e.txt  = msg;
  e.font = font;
  e.time = uint32_t(time*1000);
  e.x    = x;
  e.y    = y;
  pscreen.emplace(pscreen.begin(),std::move(e));
  update();
  }

void DialogMenu::paintEvent(Tempest::PaintEvent &e) {
  Painter p(e);

  const int dw = std::min(w(),600);
  if(txt.size()>0){
    if(tex) {
      p.setBrush(*tex);
      p.drawRect((w()-dw)/2,20,dw,100,
                 0,0,tex->w(),tex->h());
      }

    auto& t = txt.back();
    p.setFont(Resources::font());
    p.drawText((w()-dw)/2,100,t.txt.c_str());
    }

  for(size_t i=0;i<pscreen.size();++i){
    auto& sc = pscreen[i];
    p.setFont(sc.font);
    auto  sz = p.font().textSize(sc.txt.c_str());
    int x = sc.x;
    int y = sc.y;
    if(x<0){
      x = (w()-sz.w)/2;
      } else {
      x = int(w()*x/100.f);
      }
    if(y<0){
      y = (h()-sz.h)/2;
      } else {
      y = int(h()*y/100.f);
      }
    p.drawText(x,y,sc.txt.c_str());
    }
  }

void DialogMenu::onEntry(const DialogMenu::Entry &e) {
  if(e.flag&DlgClose)
    txt.clear();
  }

