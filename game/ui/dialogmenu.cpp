#include "dialogmenu.h"

#include <Tempest/Painter>
#include <Tempest/Log>
#include <algorithm>

#include "utils/gthfont.h"
#include "world/objects/npc.h"
#include "gothic.h"
#include "inventorymenu.h"
#include "resources.h"

using namespace Tempest;

bool DialogMenu::Pipe::output(Npc &npc, const Daedalus::ZString& text) {
  return owner.aiOutput(npc,text);
  }

bool DialogMenu::Pipe::outputSvm(Npc &npc, const Daedalus::ZString& text) {
  return owner.aiOutput(npc,text);
  }

bool DialogMenu::Pipe::outputOv(Npc &npc, const Daedalus::ZString& text) {
  return owner.aiOutput(npc,text);
  }

bool DialogMenu::Pipe::close() {
  return owner.aiClose();
  }

bool DialogMenu::Pipe::isFinished() {
  bool ret=false;
  owner.aiIsClose(ret);
  return ret;
  }

DialogMenu::DialogMenu(InventoryMenu &trade)
  :trade(trade), pipe(*this) {
  tex     = Resources::loadTexture("DLG_CHOICE.TGA");
  ambient = Resources::loadTexture("DLG_AMBIENT.TGA");

  setFocusPolicy(NoFocus);

  Gothic::inst().onSettingsChanged.bind(this,&DialogMenu::setupSettings);
  setupSettings();
  }

DialogMenu::~DialogMenu() {
  Gothic::inst().onSettingsChanged.ubind(this,&DialogMenu::setupSettings);
  }

void DialogMenu::setupSettings() {
  dlgAnimation = Gothic::settingsGetI("GAME","animatedWindows");
  }

void DialogMenu::tick(uint64_t dt) {
  if(state==State::PreStart){
    except.clear();
    dlgTrade=false;
    trade.close();
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

  if(isChoiseMenuActive()) {
    if(choiseAnimTime+dt>ANIM_TIME)
      choiseAnimTime = ANIM_TIME; else
      choiseAnimTime+=dt;
    } else {
    if(choiseAnimTime<dt)
      choiseAnimTime = 0; else
      choiseAnimTime-=dt;
    }

  if(current.time<=dt){
    current.time = 0;
    if(dlgTrade && !haveToWaitOutput())
      startTrade();
    } else {
    current.time-=dt;
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

void DialogMenu::drawTextMultiline(Painter &p, int x, int y, int w, int h, const std::string &txt, bool isPl) {
  auto sz = processTextMultiline(nullptr,0,0,w,h,txt,isPl);
  y+=std::max(0, (h-sz.h)/2);
  processTextMultiline(&p,x,y,w,h,txt,isPl);
  }

Size DialogMenu::processTextMultiline(Painter* p, int x, int y, int w, int h, const std::string& txt, bool isPl) {
  const int pdd=10;

  Size ret = {0,0};
  if(isPl) {
    auto& fnt = Resources::font();
    y+=fnt.pixelSize();
    if(p!=nullptr) {
      fnt.drawText(*p,x+pdd, y,
                   w-2*pdd, h, txt, Tempest::AlignHCenter);
      }
    auto sz = fnt.textSize(w,txt.c_str());
    ret.w  = std::max(ret.w,sz.w);
    ret.h += sz.h;
    } else {
    if(other!=nullptr){
      auto& fnt = Resources::font();
      y+=fnt.pixelSize();
      auto txt  = other->displayName();
      auto sz   = fnt.textSize(w,txt.data());
      if(p!=nullptr)
        fnt.drawText(*p,x+(w-sz.w)/2,y,txt.data());
      h-=int(sz.h);
      ret.w  = std::max(ret.w,sz.w);
      ret.h += sz.h;
      }
    auto& fnt = Resources::font(Resources::FontType::Yellow);
    y+=fnt.pixelSize();
    ret.h += fnt.pixelSize();
    if(p!=nullptr) {
      fnt.drawText(*p,x+pdd, y,
                   w-2*pdd, h, txt, Tempest::AlignHCenter);
      }
    auto sz = fnt.textSize(w,txt.c_str());
    ret.w  = std::max(ret.w,sz.w);
    ret.h += sz.h;
    }
  return ret;
  }

void DialogMenu::clear() {
  for(auto& i:printMsg)
    i=PScreen();
  pscreen.clear();
  }

void DialogMenu::onWorldChanged() {
  assert(state==State::Idle);
  close();
  }

bool DialogMenu::isMobsiDialog() const {
  return pl!=nullptr && other==pl && pl->interactive()!=nullptr;
  }

void DialogMenu::dialogCamera(Camera& camera) {
  if(pl && other){
    auto p0 = pl   ->cameraBone();
    auto p1 = other->cameraBone();
    camera.setPosition(0.5f*(p0.x+p1.x),
                       0.5f*(p0.y+p1.y) + 50,
                       0.5f*(p0.z+p1.z));
    p0 -= p1;

    if(pl==other) {
      float a = pl->rotation();
      camera.setSpin(PointF(0,a));
      } else {
      float l = p0.length();
      float a = 0;
      if(curentIsPl) {
        a = pl->rotation()+45-270;
        } else {
        a = other->rotation()-45+270;
        }

      camera.setDialogDistance(l);
      camera.setSpin(PointF(0,a));
      }
    }
  }

void DialogMenu::openPipe(Npc &player, Npc &npc, AiOuputPipe *&out) {
  out    = &pipe;
  pl     = &player;
  other  = &npc;
  state  = State::PreStart;
  }

bool DialogMenu::aiOutput(Npc &npc, const Daedalus::ZString& msg) {
  if(&npc!=pl && &npc!=other){
    Log::e("unexpected aiOutput call: ",msg.c_str());
    return true;
    }

  if(current.time>0)
    return false;

  if(pl==&npc) {
    if(other!=nullptr)
      other->stopDlgAnim();
    } else {
    if(pl!=nullptr)
      pl->stopDlgAnim();
    }

  current.txt     = Gothic::inst().messageByName(msg).c_str();
  current.msgTime = Gothic::inst().messageTime(msg);
  current.time    = current.msgTime + (dlgAnimation ? ANIM_TIME*2 : 0);
  currentSnd      = soundDevice.load(Resources::loadSoundBuffer(std::string(msg.c_str())+".wav"));
  curentIsPl      = (pl==&npc);

  currentSnd.play();
  update();
  return true;
  }

bool DialogMenu::aiClose() {
  if(current.time>0){
    return false;
    }

  choise.clear();
  close();
  state=State::Idle;
  update();
  return true;
  }

void DialogMenu::aiIsClose(bool &ret) {
  ret = (state==State::Idle);
  }

bool DialogMenu::isActive() const {
  return (state!=State::Idle) || current.time>0;
  }

bool DialogMenu::onStart(Npc &p, Npc &ot) {
  choise         = ot.dialogChoises(p,except,state==State::PreStart);
  state          = State::Active;
  depth          = 0;
  curentIsPl     = true;
  choiseAnimTime = 0;

  if(choise.size()>0 && choise[0].title.size()==0){
    // important dialog
    onEntry(choise[0]);
    }

  dlgSel=0;
  update();
  return true;
  }

void DialogMenu::printScreen(std::string_view msg, int x, int y, int time, const GthFont &font) {
  PScreen e;
  e.txt  = msg;
  e.font = &font;
  e.time = uint32_t(time*1000)+1000;
  e.x    = x;
  e.y    = y;
  pscreen.emplace(pscreen.begin(),std::move(e));
  update();
  }

void DialogMenu::print(std::string_view msg) {
  if(msg.empty())
    return;

  for(size_t i=1;i<MAX_PRINT;++i)
    printMsg[i-1u]=printMsg[i];

  remPrint=1500;
  PScreen& e=printMsg[MAX_PRINT-1];
  e.txt  = msg;
  e.font = &Resources::font();
  e.time = remPrint;
  e.x    = -1;
  e.y    = -1;
  update();
  }

void DialogMenu::onDoneText() {
  choise = Gothic::inst().updateDialog(selected,*pl,*other);
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
  auto prevPl  = pl;
  auto prevNpc = other;

  pl=nullptr;
  other=nullptr;
  depth=0;
  dlgTrade=false;
  current.time=0;
  choise.clear();
  state=State::Idle;
  currentSnd = SoundEffect();
  update();

  if(prevPl!=nullptr && prevPl==prevNpc){
    auto i = prevPl->interactive();
    if(i!=nullptr)
      prevPl->setInteraction(nullptr);
    }
  if(prevPl){
    prevPl->stopDlgAnim();
    }
  if(prevNpc && prevNpc!=prevPl){
    prevNpc->stopDlgAnim();
    }
  }

bool DialogMenu::haveToWaitOutput() const {
  if(pl && pl->haveOutput()){
    return true;
    }
  if(other && other->haveOutput()){
    return true;
    }
  return false;
  }

void DialogMenu::startTrade() {
  if(pl!=nullptr && other!=nullptr)
    trade.trade(*pl,*other);
  dlgTrade=false;
  }

void DialogMenu::skipPhrase() {
  if(current.time>0) {
    currentSnd   = SoundEffect();
    current.time = 1;
    }
  }

void DialogMenu::onEntry(const GameScript::DlgChoise &e) {
  if(pl && other) {
    selected = e;
    depth    = 1;
    dlgTrade = e.isTrade;
    except.push_back(e.scriptFn);
    Gothic::inst().dialogExec(e,*pl,*other);

    onDoneText();
    }
  }

void DialogMenu::paintEvent(Tempest::PaintEvent &e) {
  Painter p(e);
  const uint64_t da = dlgAnimation ? ANIM_TIME : 0;
  const int      dw = std::min(w(),600);
  const int      dh = 100;

  if(current.time>0 && trade.isOpen()==InventoryMenu::State::Closed) {
    if(ambient!=nullptr) {
      int dlgW = dw;
      int dlgH = dh;

      if(current.time>current.msgTime+da) {
        float k = 1.f-float(current.time-current.msgTime-da)/float(da);
        dlgW = int(float(dlgW)*k);
        dlgH = int(float(dlgH)*k);
        }
      else if(current.time<da) {
        float k = float(current.time)/float(da);
        dlgW = int(float(dlgW)*k);
        dlgH = int(float(dlgH)*k);
        }

      p.setBrush(*ambient);
      p.drawRect((w()-dlgW)/2, 20+(dh-dlgH)/2, dlgW, dlgH,
                 0,0,ambient->w(),ambient->h());
      }

    if(current.time>da && current.time<current.msgTime+da)
      drawTextMultiline(p,(w()-dw)/2,20,dw,dh,current.txt,curentIsPl);
    }

  paintChoise(e);

  for(size_t i=0;i<MAX_PRINT;++i){
    auto& sc  = printMsg[i];
    if(sc.font==nullptr)
      continue;

    auto& fnt = *sc.font;
    auto  sz  = fnt.textSize(sc.txt);
    int   x   = (w()-sz.w)/2;
    fnt.drawText(p, x, int(i*2+2)*sz.h, sc.txt);
    }

  for(size_t i=0;i<pscreen.size();++i){
    auto& sc   = pscreen[i];
    auto& fnt  = *sc.font;
    auto  sz   = fnt.textSize(sc.txt);
    auto  area = this->size();

    int x = sc.x;
    int y = sc.y;
    if(x<0){
      x = (area.w-sz.w)/2;
      } else {
      x = (area.w*x)/100;
      }
    if(y<0){
      y = (area.h-sz.h)/2;
      } else {
      y = ((area.h-sz.h)*y)/100+sz.h;
      }
    fnt.drawText(p, x, y, sc.txt);
    }
  }

void DialogMenu::paintChoise(PaintEvent &e) {
  if(choise.size()==0)
    return;

  auto& fnt = Resources::dialogFont();
  const int  padd     = 20;
  const int  dw       = std::min(w(),600);
  const bool isActive = isChoiseMenuActive();

  int  dh = padd*2;
  for(size_t i=0;i<choise.size();++i){
    const GthFont* font = &fnt;
    Size choiseTextSize = font->textSize(dw-padd, choise[i].title.c_str());
    dh += choiseTextSize.h;
    }

  const int  y        = h()-dh-20;

  if(tex!=nullptr && (choiseAnimTime>0 || isActive)) {
    float k    = dlgAnimation ? (float(choiseAnimTime)/float(ANIM_TIME)) : 1.f;
    int   dlgW = int(float(dw)*k);
    int   dlgH = int(float(dh)*k);

    Painter p(e);
    p.setBrush(*tex);
    p.drawRect((w()-dlgW)/2,y+(dh-dlgH)/2,dlgW,dlgH,
               0,0,tex->w(),tex->h());
    }

  if(choiseAnimTime<ANIM_TIME && dlgAnimation)
    return;

  if(!isChoiseMenuActive())
    return;

  int textHeightOffset = padd;
  Painter p(e);
  for(size_t i=0;i<choise.size();++i){
    const GthFont* font = &fnt;
    int x = (w()-dw)/2;
    if(i==dlgSel)
      font = &Resources::font(Resources::FontType::Hi);
    Size choiseTextSize = font->textSize(dw-padd, choise[i].title.c_str());
    font->drawText(p,x+padd,y+padd+textHeightOffset,dw-padd,0,choise[i].title,AlignLeft);
    textHeightOffset += choiseTextSize.h;
    }
  }

bool DialogMenu::isChoiseMenuActive() const {
  return choise.size()!=0 && current.time==0 && !haveToWaitOutput() && trade.isOpen()==InventoryMenu::State::Closed;
  }

void DialogMenu::onSelect() {
  if(current.time>0 || haveToWaitOutput()){
    return;
    }

  if(dlgSel<choise.size()) {
    onEntry(choise[dlgSel]);
    choiseAnimTime = dlgAnimation ? ANIM_TIME : 0;
    }
  }

void DialogMenu::mouseDownEvent(MouseEvent &event) {
  if(state==State::Idle || trade.isOpen()!=InventoryMenu::State::Closed){
    event.ignore();
    return;
    }

  if(event.button==MouseEvent::ButtonLeft) {
    onSelect();
    }
  if(event.button==MouseEvent::ButtonRight) {
    skipPhrase();
    }
  }

void DialogMenu::mouseWheelEvent(MouseEvent &e) {
  if(state==State::Idle || trade.isOpen()!=InventoryMenu::State::Closed){
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

void DialogMenu::keyDownEvent(KeyEvent &e) {
  if(state==State::Idle || trade.isOpen()!=InventoryMenu::State::Closed){
    e.ignore();
    return;
    }

  if(e.key==Event::K_Return){
    onSelect();
    return;
    }
  if(e.key==Event::K_W || e.key==Event::K_S || e.key==Event::K_Up || e.key==Event::K_Down || e.key==Event::K_ESCAPE){
    if(e.key==Event::K_W || e.key==Event::K_Up){
      dlgSel--;
      }
    if(e.key==Event::K_S || e.key==Event::K_Down){
      dlgSel++;
      }
    if(e.key==Event::K_ESCAPE){
      skipPhrase();
      }
    dlgSel = (dlgSel+choise.size())%std::max<size_t>(choise.size(),1);
    update();
    return;
    }
  e.ignore();
  }

void DialogMenu::keyUpEvent(KeyEvent &event) {
  if(state==State::Idle && current.time>0){
    event.ignore();
    return;
    }
  }

