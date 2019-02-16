#include "dialogmenu.h"

#include <Tempest/Painter>
#include <algorithm>

#include "gothic.h"
#include "resources.h"

using namespace Tempest;

DialogMenu::DialogMenu(Gothic &gothic):gothic(gothic) {
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
      onDoneText();
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
  e.time = uint32_t(e.txt.size()*50);
  if(txt.size()==0)
    remDlg=e.time;
  txt.emplace(txt.begin(),std::move(e));
  update();
  }

void DialogMenu::aiClose() {
  Entry e;
  e.flag = DlgClose;
  txt.emplace(txt.begin(),std::move(e));
  choise.clear();
  update();
  }

void DialogMenu::start(std::vector<WorldScript::DlgChoise> &&c, Npc &p, Npc &ot) {
  choise=std::move(c);
  pl    = &p;
  other = &ot;
  if(choise.size()>0 && choise[0].title.size()==0){
    // important dialog
    onEntry(choise[0]);
    }
  dlgSel=0;
  setFocus(true);
  update();
  }

void DialogMenu::printScreen(const char *msg, int x, int y, int time, const Tempest::Font &font) {
  PScreen e;
  e.txt  = msg;
  e.font = font;
  e.time = uint32_t(time*1000)+1000;
  e.x    = x;
  e.y    = y;
  pscreen.emplace(pscreen.begin(),std::move(e));
  update();
  }

void DialogMenu::onDoneText() {
  choise = gothic.updateDialog(selected);
  dlgSel = 0;
  update();
  }

void DialogMenu::onEntry(const DialogMenu::Entry &e) {
  if(e.flag&DlgClose){
    txt.clear();
    choise.clear();
    owner()->setFocus(true);
    return;
    }
  }

void DialogMenu::onEntry(const WorldScript::DlgChoise &e) {
  if(pl && other) {
    selected=e;
    gothic.dialogExec(e,*pl,*other);
    }
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
    p.setFont(Resources::dialogFont());
    p.drawText((w()-dw)/2,100,t.txt.c_str());
    }

  paintChoise(e);

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

void DialogMenu::paintChoise(PaintEvent &e) {
  if(choise.size()==0 || txt.size()>0)
    return;

  Painter p(e);
  p.setFont(Resources::dialogFont());
  const int padd = 20;
  const int dw   = std::min(w(),600);
  const int dh   = int(choise.size()*p.font().pixelSize())+2*padd;
  const int y    = h()-dh-20;

  if(tex) {
    p.setBrush(*tex);
    p.drawRect((w()-dw)/2,y,dw,dh,
               0,0,tex->w(),tex->h());
    }

  for(size_t i=0;i<choise.size();++i){
    int x = (w()-dw)/2;
    if(i==dlgSel)
      p.setBrush(Color(1.f)); else
      p.setBrush(Color(0.6f,0.6f,0.6f,1.f));
    p.drawText(x+padd,y+padd+int((i+1)*p.font().pixelSize()),choise[i].title.c_str());
    }
  }

void DialogMenu::mouseDownEvent(MouseEvent &event) {
  if(choise.size()==0){
    event.ignore();
    return;
    }

  if(txt.size()){
    event.accept();
    return;
    }

  if(dlgSel<choise.size())
    onEntry(choise[dlgSel]);
  event.accept();
  }

void DialogMenu::mouseWheelEvent(MouseEvent &e) {
  if(e.delta>0)
    dlgSel--;
  if(e.delta<0)
    dlgSel++;
  dlgSel = (dlgSel+choise.size())%std::max<size_t>(choise.size(),1);
  update();
  }

void DialogMenu::keyDownEvent(KeyEvent &event) {
  if(event.key==Event::K_ESCAPE){
    if(txt.size()>0){
      remDlg=0;
      update();
      }
    }
  }

