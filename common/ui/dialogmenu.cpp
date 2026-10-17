#include "dialogmenu.h"
#include "ui/cursorshape.h"

#include <Tempest/Painter>
#include <Tempest/Log>
#include <algorithm>
#include <cassert>

#include "utils/gthfont.h"
#include "utils/string_frm.h"
#include "world/objects/npc.h"
#include "gothic.h"
#include "inventorymenu.h"
#include "resources.h"

using namespace Tempest;

bool DialogMenu::Pipe::output(Npc &npc, std::string_view text) {
  return owner.aiOutput(npc,text);
  }

bool DialogMenu::Pipe::outputSvm(Npc &npc, std::string_view text) {
  return owner.aiOutput(npc,text);
  }

bool DialogMenu::Pipe::outputOv(Npc &npc, std::string_view text) {
  return owner.aiOutput(npc,text);
  }

bool DialogMenu::Pipe::printScr(Npc& npc, int time, std::string_view msg, int x, int y, std::string_view font) {
  return owner.aiPrintScr(npc,time,msg,x,y,font);
  }

bool DialogMenu::Pipe::close() {
  return owner.aiClose();
  }

bool DialogMenu::Pipe::isFinished() {
  return !owner.isNpcInDialog(nullptr);
  }

DialogMenu::DialogMenu(InventoryMenu &trade)
  :trade(trade), pipe(*this) {
  tex     = Resources::loadTexture("DLG_CHOICE.TGA");
  ambient = Resources::loadTexture("DLG_AMBIENT.TGA");

  setCursorShape(HiddenCursor);
  setFocusPolicy(NoFocus);

  Gothic::inst().onSettingsChanged.bind(this,&DialogMenu::setupSettings);
  setupSettings();
  }

DialogMenu::~DialogMenu() {
  Gothic::inst().onSettingsChanged.ubind(this,&DialogMenu::setupSettings);
  }

void DialogMenu::setupSettings() {
  dlgAnimation        = Gothic::settingsGetI("GAME","animatedWindows");
  showSubtitles       = Gothic::settingsGetI("GAME","subTitles");
  showSubtitlesPlayer = Gothic::settingsGetI("GAME","subTitlesPlayer");

  const float volume = Gothic::settingsSoundVolume();
  soundDevice.setGlobalVolume(volume);
  }

void DialogMenu::tick(uint64_t dt) {
  if(state==State::PreStart) {
    except.clear();
    dlgTrade=false;
    trade.close();
    onStart(*this->pl,*this->other);
    return;
    }

  if(remPrint>0 || choiceAnimTime>0 || current.time>0 || pscreen.size()>0)
    update();

  if(remPrint<dt) {
    for(size_t i=1;i<MAX_PRINT;++i)
      printMsg[i-1u]=printMsg[i];
    printMsg[MAX_PRINT-1]=PScreen();
    remPrint=1500;
    } else {
    remPrint-=dt;
    }

  if(isChoiceMenuActive()) {
    if(choiceAnimTime+dt>ANIM_TIME)
      choiceAnimTime = ANIM_TIME; else
      choiceAnimTime+=dt;
    } else {
    if(choiceAnimTime<dt)
      choiceAnimTime = 0; else
      choiceAnimTime-=dt;
    }

  if(current.time<=dt){
    current.time = 0;
    if(dlgTrade && !haveToWaitOutput()) {
      startTrade();
      }
    else if(choice.size()==0 && state!=State::Idle && !haveToWaitOutput()) {
      close();
      }
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

  // update();
  }

void DialogMenu::drawTextMultiline(Painter &p, int x, int y, int w, int h, std::string_view txt, bool isPl) {
  auto sz = processTextMultiline(nullptr,0,0,w,h,txt,isPl);
  y+=std::max(0, (h-sz.h)/2);
  processTextMultiline(&p,x,y,w,h,txt,isPl);
  }

Size DialogMenu::processTextMultiline(Painter* p, int x, int y, int w, int h, std::string_view txt, bool isPl) {
  const float scale = Gothic::interfaceScale(this);
  const int   pdd   = int(10*scale);

  auto fnt = Resources::font(scale);
  Size ret = {0,0};
  if(!isPl && other!=nullptr) {
    y += fnt.pixelSize();
    auto txt  = other->displayName();
    auto sz   = fnt.textSize(w,txt.data());
    if(p!=nullptr)
      fnt.drawText(*p,x+(w-sz.w)/2,y,txt.data());
    h     -= int(sz.h);
    ret.w  = std::max(ret.w,sz.w);
    ret.h += sz.h;
    }

  fnt = Resources::font(isPl ? Resources::FontType::Normal : Resources::FontType::Yellow, scale);

  y     += fnt.pixelSize();
  ret.h += fnt.pixelSize();
  if(p!=nullptr) {
    fnt.drawText(*p,x+pdd, y,
                 w-2*pdd, h, txt, Tempest::AlignHCenter);
    }
  auto sz = fnt.textSize(w,txt);
  ret.w  = std::max(ret.w,sz.w);
  ret.h += sz.h;

  return ret;
  }

void DialogMenu::clear() {
  for(auto& i:printMsg)
    i=PScreen();
  pscreen.clear();
  }

void DialogMenu::onWorldChanged() {
  close();
  clear();
  }

bool DialogMenu::isMobsiDialog() const {
  return pl!=nullptr && other==pl && pl->interactive()!=nullptr;
  }

void DialogMenu::dialogCamera(Camera& camera) {
  if(pl && other){
    auto p0 = pl   ->cameraBone();
    auto p1 = other->cameraBone();
    camera.setTarget((p0+p1)*0.5f + Vec3(0,50,0));
    p0 -= p1;

    if(pl==other) {
      // float a = pl->rotation();
      // camera.setSpin(PointF(0,a));
      } else {
      float l = p0.length();
      float a = 0;
      if(curentIsPl) {
        a = other->rotation()-45;
        } else {
        a = pl->rotation()+45;
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

bool DialogMenu::isNpcInDialog(const Npc* npc) const {
  if(state==State::Idle)
    return false;
  return npc==pl || npc==other || npc==nullptr;
  }

bool DialogMenu::aiOutput(Npc &npc, std::string_view msg) {
  if(&npc!=pl && &npc!=other){
    Log::e("unexpected aiOutput call: ",msg.data());
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

  current.txt     = Gothic::inst().messageByName(msg);
  current.msgTime = Gothic::inst().messageTime(msg);
  current.time    = current.msgTime + (dlgAnimation ? ANIM_TIME*2 : 0);
  currentSnd      = soundDevice.load(Resources::loadSoundBuffer(string_frm(msg,".wav")));
  curentIsPl      = (pl==&npc);

  currentSnd.play();
  npc.setAiOutputBarrier(current.msgTime,false);
  update();
  return true;
  }

bool DialogMenu::aiPrintScr(Npc& npc, int time, std::string_view msg, int x, int y, std::string_view font) {
  if(current.time>0)
    return false;
  auto& f = Resources::font(font, Resources::FontType::Normal, 1.0);
  Gothic::inst().onPrintScreen(msg,x,y,time,f);
  return true;
  }

bool DialogMenu::aiClose() {
  if(current.time>0){
    return false;
    }

  choice.clear();
  close();
  state=State::Idle;
  update();
  return true;
  }

bool DialogMenu::isActive() const {
  return (state!=State::Idle) || current.time>0;
  }

bool DialogMenu::hasContent() const {
  return (current.time>0 || choice.size()>0);
  }

bool DialogMenu::onStart(Npc &p, Npc &ot) {
  choice         = ot.dialogChoices(p,except,state==State::PreStart);
  state          = State::Active;
  depth          = 0;
  curentIsPl     = true;
  choiceAnimTime = 0;

  if(choice.size()>0 && choice[0].title.size()==0){
    // important dialog
    onEntry(choice[0]);
    }

  dlgSel=0;
  update();
  return true;
  }

void DialogMenu::printScreen(std::string_view msg, int x, int y, int time, const GthFont &font) {
  if(x>=0 && y>=0) {
    for(auto& sc:pscreen) {
      if(sc.x==x && sc.y==y) {
        sc.txt  = msg;
        sc.font = &font;
        sc.time = uint32_t(time*1000);
        return;
        }
      }
    }
  PScreen e;
  e.txt  = msg;
  e.font = &font;
  e.time = uint32_t(time*1000);
  e.x    = x;
  e.y    = y;
  pscreen.emplace_back(std::move(e));
  update();
  }

void DialogMenu::drawMsg(Tempest::Painter& p, int offsetY) {
  for(size_t i=0;i<MAX_PRINT;++i){
    auto& sc  = printMsg[i];
    if(sc.font==nullptr)
      continue;

    auto& fnt = *sc.font;
    auto  sz  = fnt.textSize(sc.txt);
    int   x   = (w()-sz.w)/2;
    fnt.drawText(p, x, offsetY + int(i*2+1)*sz.h, sc.txt);
    }
  }

void DialogMenu::print(std::string_view msg) {
  if(msg.empty())
    return;

  const float scale = Gothic::interfaceScale(this);
  for(size_t i=1;i<MAX_PRINT;++i)
    printMsg[i-1u]=printMsg[i];

  remPrint=1500;
  PScreen& e=printMsg[MAX_PRINT-1];
  e.txt  = msg;
  e.font = &Resources::font(scale);
  e.time = remPrint;
  e.x    = -1;
  e.y    = -1;
  update();
  }

void DialogMenu::onDoneText() {
  choice = Gothic::inst().updateDialog(selected,*pl,*other);
  dlgSel = 0;
  if(choice.size()==0){
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
  choice.clear();
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

bool DialogMenu::haveToShowSubtitles(bool isPl) const {
  return showSubtitles && (showSubtitlesPlayer || !isPl);
  }

void DialogMenu::startTrade() {
  if(pl!=nullptr && other!=nullptr)
    trade.trade(*pl,*other);
  dlgTrade=false;
  }

void DialogMenu::skipPhrase() {
  if(current.time>0) {
    if(pl!=nullptr)
      pl->setAiOutputBarrier(0,false);
    if(other!=nullptr)
      other->setAiOutputBarrier(0,false);
    currentSnd   = SoundEffect();
    current.time = 1;
    }
  }

void DialogMenu::onEntry(const GameScript::DlgChoice &e) {
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
  const float scale = Gothic::interfaceScale(this);

  Painter p(e);
  const uint64_t da = dlgAnimation ? ANIM_TIME : 0;
  const int      dw = std::min(w(),int(600*scale));
  const int      dh = int(100*scale);

  if(current.time>0 && trade.isOpen()==InventoryMenu::State::Closed && haveToShowSubtitles(curentIsPl)) {
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

  paintChoice(e);

  for(size_t i=0;i<pscreen.size();++i){
    auto& sc   = pscreen[i];
    auto  fnt  = *sc.font;
    fnt.setScale(Gothic::interfaceScale(this));

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

#ifdef OPENGOTHIC_TOUCH_UI
/// Answers are small cards at the bottom of the screen. When there are many of them, the list is scrolled with a finger.
DialogMenu::TouchGeometry DialogMenu::touchGeometry() const {
  TouchGeometry g;
  if(choice.empty())
    return g;

  const float scale = Gothic::interfaceScale(this)*std::max(1.f,float(h())/700.f);
  auto        fnt   = Resources::dialogFont(scale);
  const int   padd  = std::max(6,int(float(fnt.pixelSize())*0.35f));
  const int   gap   = std::max(4,int(float(h())*0.008f));
  const int   cw    = int(float(w())*0.5f);
  const int   minH  = int(float(fnt.pixelSize())*1.9f);

  int y = 0;
  for(auto& c:choice) {
    const int th = fnt.textSize(cw-2*padd,c.title).h;
    const int ch = std::max(minH,th+2*padd);
    g.cards.push_back(Rect(0,y,cw,ch));
    y += ch+gap;
    }
  g.content = std::max(0,y-gap);

  const int viewH = std::min(g.content,int(float(h())*0.36f));
  g.view      = Rect((w()-cw)/2,h()-int(float(h())*0.04f)-viewH,cw,viewH);
  g.maxScroll = g.content-viewH;
  g.padd      = padd;
  g.scale     = scale;
  return g;
  }

void DialogMenu::paintTouchChoice(PaintEvent& e) {
  if(!isChoiceMenuActive())
    return;

  const auto geo = touchGeometry();
  touchScroll = std::clamp(touchScroll,0.f,float(geo.maxScroll));

  Painter p(e);
  p.setScissor(geo.view.x,geo.view.y,geo.view.w,geo.view.h);
  for(size_t i=0; i<choice.size() && i<geo.cards.size(); ++i) {
    const int  x        = geo.view.x;
    const int  y        = geo.view.y+geo.cards[i].y-int(touchScroll);
    const int  cw       = geo.cards[i].w;
    const int  ch       = geo.cards[i].h;
    if(y+ch<geo.view.y || y>geo.view.y+geo.view.h)
      continue;
    const bool selected = i==dlgSel;
    const int  border   = selected ? 2 : 1;

    p.setBrush(Brush(selected ? Color(0.86f,0.68f,0.30f,0.95f) : Color(0.48f,0.44f,0.38f,0.70f)));
    p.drawRect(x,y,cw,ch);
    p.setBrush(Brush(selected ? Color(0.11f,0.085f,0.04f,0.92f) : Color(0.02f,0.025f,0.035f,0.80f)));
    p.drawRect(x+border,y+border,cw-2*border,ch-2*border);

    GthFont fnt = selected ? Resources::font(Resources::FontType::Hi,geo.scale) : Resources::dialogFont(geo.scale);
    const int tw = cw-2*geo.padd;
    const int th = fnt.textSize(tw,choice[i].title).h;
    fnt.drawText(p,x+geo.padd,y+(ch-th)/2+fnt.pixelSize(),tw,0,choice[i].title,AlignLeft);
    }

  // the list can be scrolled
  if(geo.maxScroll>0) {
    const int   bw     = std::max(3,int(geo.scale*3));
    const float k      = float(geo.view.h)/float(geo.content);
    const int   th     = std::max(bw*4,int(float(geo.view.h)*k));
    const int   ty     = geo.view.y+int(float(geo.view.h-th)*touchScroll/float(geo.maxScroll));
    p.setBrush(Brush(Color(0.86f,0.68f,0.30f,0.75f)));
    p.drawRect(geo.view.x+geo.view.w-bw-1,ty,bw,th);
    }
  p.setScissor(0,0,w(),h());
  }

void DialogMenu::mobileTouch(float x, float y, int phase) {
  if(!isChoiceMenuActive()) {
    touchDrag = false;
    return;
    }

  const auto geo = touchGeometry();
  if(phase==0) {
    touchDrag        = geo.maxScroll>0 && geo.view.contains(int(x),int(y));
    touchStartY      = y;
    touchStartScroll = touchScroll;
    return;
    }
  if(!touchDrag)
    return;
  if(phase>=2) {
    touchDrag = false;
    return;
    }

  // a tap that moves a little must not scroll the list
  float dy = touchStartY-y;
  const float dead = float(geo.padd);
  dy = dy>dead ? dy-dead : dy<-dead ? dy+dead : 0.f;
  touchScroll = std::clamp(touchStartScroll+dy,0.f,float(geo.maxScroll));
  update();
  }

void DialogMenu::mobileTap(float x, float y) {
  if(state==State::Idle || trade.isOpen()!=InventoryMenu::State::Closed)
    return;

  if(isChoiceMenuActive()) {
    const auto geo = touchGeometry();
    if(!geo.view.contains(int(x),int(y)))
      return;
    for(size_t i=0; i<geo.cards.size(); ++i) {
      Rect r = geo.cards[i];
      r.x += geo.view.x;
      r.y += geo.view.y-int(touchScroll);
      if(!r.contains(int(x),int(y)))
        continue;
      dlgSel = i;
      onSelect();
      update();
      return;
      }
    return;
    }

  // somebody is talking
  if(current.time>0)
    skipPhrase();
  }
#endif

void DialogMenu::paintChoice(PaintEvent &e) {
#ifdef OPENGOTHIC_TOUCH_UI
  paintTouchChoice(e);
  return;
#else
  if(choice.size()==0)
    return;

  const float scale    = Gothic::interfaceScale(this);
  const int   padd     = int(20*scale);
  const int   dw       = std::min(w(),int(600*scale));
  const bool  isActive = isChoiceMenuActive();

  int  dh = padd*2;
  for(size_t i=0;i<choice.size();++i){
    auto fnt = Resources::dialogFont(scale);
    Size choiceTextSize = fnt.textSize(dw-padd, choice[i].title);
    dh += choiceTextSize.h;
    }

  const int  y        = h()-dh-int(20*scale);

  if(tex!=nullptr && (choiceAnimTime>0 || isActive)) {
    float k    = dlgAnimation ? (float(choiceAnimTime)/float(ANIM_TIME)) : 1.f;
    int   dlgW = int(float(dw)*k);
    int   dlgH = int(float(dh)*k);

    Painter p(e);
    p.setBrush(*tex);
    p.drawRect((w()-dlgW)/2,y+(dh-dlgH)/2,dlgW,dlgH,
               0,0,tex->w(),tex->h());
    }

  if(choiceAnimTime<ANIM_TIME && dlgAnimation)
    return;

  if(!isChoiceMenuActive())
    return;

  int textHeightOffset = padd;
  Painter p(e);
  for(size_t i=0;i<choice.size();++i){
    GthFont fnt;
    if(i==dlgSel)
      fnt = Resources::font(Resources::FontType::Hi, scale); else
      fnt = Resources::dialogFont(scale);

    int x = (w()-dw)/2;
    Size choiceTextSize = fnt.textSize(dw-padd, choice[i].title);
    fnt.drawText(p,x+padd,y+padd+textHeightOffset,dw-padd,0,choice[i].title,AlignLeft);
    textHeightOffset += choiceTextSize.h;
    }
#endif
  }

bool DialogMenu::isChoiceMenuActive() const {
  return choice.size()!=0 && current.time==0 && !haveToWaitOutput() && trade.isOpen()==InventoryMenu::State::Closed;
  }

void DialogMenu::onSelect() {
  if(current.time>0 || haveToWaitOutput()){
    return;
    }

  if(dlgSel<choice.size()) {
    onEntry(choice[dlgSel]);
    choiceAnimTime = dlgAnimation ? ANIM_TIME : 0;
    }
  }

void DialogMenu::mouseDownEvent(MouseEvent &event) {
#ifdef OPENGOTHIC_TOUCH_UI
  // Dialog is controlled by touch buttons
  event.ignore();
  return;
#endif
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
  dlgSel = (dlgSel+choice.size())%std::max<size_t>(choice.size(),1);
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
    dlgSel = (dlgSel+choice.size())%std::max<size_t>(choice.size(),1);
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

