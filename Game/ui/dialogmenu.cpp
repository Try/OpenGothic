#include "dialogmenu.h"

#include <Tempest/Painter>
#include <algorithm>

#include "gothic.h"
#include "resources.h"

using namespace Tempest;

DialogMenu::DialogMenu(Gothic &gothic):gothic(gothic) {
  tex     = Resources::loadTexture("DLG_CHOICE.TGA");
  ambient = Resources::loadTexture("DLG_AMBIENT.TGA");
  setFocusPolicy(NoFocus);
  }

void DialogMenu::tick(uint64_t dt) {
  if(state==State::PreStart){
    except.clear();
    onStart(*this->pl,*this->other);
    return;
    }

  if(remPrint<dt){
    for(size_t i=1;i<MAX_PRINT;++i)
      printMsg[i-1u]=printMsg[i];
    printMsg[MAX_PRINT-1]=PScreen();
    remPrint=1500;
    } else {
    remPrint-=dt;
    }

  if(txt.size()>0){
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

bool DialogMenu::start(Npc &pl,Npc &other) {
  other.startDialog(&pl);
  return true;
  }

bool DialogMenu::start(Npc &pl, Interactive &other) {
  pl.setInteraction(&other);
  return true;
  }

void DialogMenu::aiProcessInfos(Npc &p,Npc &npc) {
  pl     = &p;
  other  = &npc;
  state  = State::PreStart;
  }

void DialogMenu::aiOutput(Npc &player,const char *msg) {
  if(&player!=pl && &player!=other)
    return; // vatras is here
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
  state=State::Idle;
  update();
  }

void DialogMenu::aiIsClose(bool &ret) {
  ret = (state==State::Idle);
  }

bool DialogMenu::isActive() const {
  return (state!=State::Idle);
  }

bool DialogMenu::onStart(Npc &p, Npc &ot) {
  pl     = &p;
  other  = &ot;
  choise = ot.dialogChoises(p,except);
  state  = State::Active;
  depth  = 0;

  if(choise.size()==0){
    close();
    return false;
    }

  if(choise.size()>0 && choise[0].title.size()==0){
    // important dialog
    onEntry(choise[0]);
    /*
    for(auto& c:choise){
      onEntry(c);
      break;
      }*/
    }

  dlgSel=0;
  setFocus(true);
  update();
  return true;
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

void DialogMenu::print(const char *msg) {
  if(msg==nullptr || msg[0]=='\0')
    return;

  for(size_t i=1;i<MAX_PRINT;++i)
    printMsg[i-1u]=printMsg[i];

  remPrint=1500;
  PScreen& e=printMsg[MAX_PRINT-1];
  e.txt  = msg;
  e.font = Resources::font();
  e.time = remPrint;
  e.x    = -1;
  e.y    = -1;
  update();
  }

void DialogMenu::onDoneText() {
  choise = gothic.updateDialog(selected,*pl,*other);
  dlgSel = 0;
  if(choise.size()==0){
    if(depth>0) {
      onStart(*pl,*other);
      } else {
      close();
      }
    }
  }

void DialogMenu::close() {
  auto prevPl   = pl;
  auto proveNpc = other;

  pl=nullptr;
  other=nullptr;
  depth=0;
  txt.clear();
  choise.clear();
  owner()->setFocus(true);
  state=State::Idle;
  update();

  if(prevPl){
    prevPl->setInteraction(nullptr);
    }
  if(proveNpc && proveNpc!=prevPl){
    //proveNpc->tickState();
    //proveNpc->startState(Npc::State::INVALID,nullptr);
    proveNpc->setInteraction(nullptr);
    }
  }

void DialogMenu::onEntry(const DialogMenu::Entry &e) {
  if(e.flag&DlgClose){
    close();
    return;
    }
  }

void DialogMenu::onEntry(const WorldScript::DlgChoise &e) {
  if(pl && other) {
    selected = e;
    depth    = 1;
    except.push_back(e.scriptFn);
    gothic.dialogExec(e,*pl,*other);
    if(txt.empty()) {
      onDoneText(); // 'BACK' menu
      } else {
      onEntry(txt.back());
      }
    }
  }

void DialogMenu::paintEvent(Tempest::PaintEvent &e) {
  Painter p(e);

  const int dw = std::min(w(),600);
  if(txt.size()>0){
    if(ambient) {
      p.setBrush(*ambient);
      p.drawRect((w()-dw)/2,20,dw,100,
                 0,0,ambient->w(),ambient->h());
      }

    auto& t = txt.back();
    p.setFont(Resources::dialogFont());
    p.drawText((w()-dw)/2,100,t.txt);
    }

  paintChoise(e);

  for(size_t i=0;i<MAX_PRINT;++i){
    auto& sc = printMsg[i];
    p.setFont(sc.font);
    auto  sz = p.font().textSize(sc.txt);
    int x = (w()-sz.w)/2;
    p.drawText(x, int(i*2+2)*sz.h, sc.txt);
    }

  for(size_t i=0;i<pscreen.size();++i){
    auto& sc = pscreen[i];
    p.setFont(sc.font);
    auto  sz = p.font().textSize(sc.txt);
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
    p.drawText(x,y,sc.txt);
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
    p.drawText(x+padd,y+padd+int((i+1)*p.font().pixelSize()),choise[i].title);
    }
  }

void DialogMenu::mouseDownEvent(MouseEvent &event) {
  if(state==State::Idle){
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
  if(state==State::Idle){
    e.ignore();
    return;
    }

  if(e.delta>0)
    dlgSel--;
  if(e.delta<0)
    dlgSel++;
  dlgSel = (dlgSel+choise.size())%std::max<size_t>(choise.size(),1);
  update();
  }

void DialogMenu::keyDownEvent(KeyEvent &event) {
  if(state==State::Idle && txt.size()==0){
    event.ignore();
    return;
    }

  if(event.key==Event::K_ESCAPE){
    if(txt.size()>0){
      remDlg=0;
      update();
      }
    }
  }

