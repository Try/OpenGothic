#include "dialogmenu.h"

#include <Tempest/Painter>
#include <Tempest/Log>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <map>

#include "utils/gthfont.h"
#include "utils/string_frm.h"
#include "world/objects/npc.h"
#include "world/world.h"
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

  // gothic-diablo: the ARPG mode keeps the mouse cursor (answers are picked with it)
  setCursorShape(Camera::isArpgEnabled() ? CursorShape::Arrow : CursorShape::Hidden);
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

  if(thirdLeft>0) {
    thirdLeft = (thirdLeft>dt) ? thirdLeft-dt : 0;
    if(thirdLeft==0 && third!=nullptr) {
      third->arpgLookAtHead(nullptr, 0);
      third = nullptr;
      }
    update();
    }
  tickActing(dt);
  {
    // gothic-diablo: the letterbox comes in and goes out in 0.3 s
    const float dst = (isArpgCinema() && trade.isOpen()==InventoryMenu::State::Closed) ? 1.f : 0.f; // not over the trade
    const float stp = float(dt)/300.f;
    if(barsK!=dst) {
      barsK = std::clamp(dst, barsK-stp, barsK+stp);
      update();
      }
  }

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

  lineId++;
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
  stopActing();
  third     = nullptr;
  thirdLeft = 0;
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

  if(Camera::isArpgEnabled()) {
    paintArpg(p);
    paintScreenMessages(p);
    return;
    }

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
  paintScreenMessages(p);
  }

void DialogMenu::paintScreenMessages(Painter& p) {
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

void DialogMenu::paintChoice(PaintEvent &e) {
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
  }

bool DialogMenu::isChoiceMenuActive() const {
  return choice.size()!=0 && current.time==0 && thirdLeft==0 && !haveToWaitOutput() && trade.isOpen()==InventoryMenu::State::Closed;
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
  if(state==State::Idle || trade.isOpen()!=InventoryMenu::State::Closed){
    event.ignore();
    return;
    }

  if(Camera::isArpgEnabled()) {
    // gothic-diablo: left button: the answer under the cursor, or the next line; right button: the next line
    if(event.button==MouseEvent::ButtonLeft && arpgChoiceReady()) {
      for(size_t i=0; i<choiceRects.size() && i<choice.size(); ++i)
        if(choiceRects[i].contains(event.pos())) {
          dlgSel = i;
          onSelect();
          return;
          }
      return;
      }
    if(event.button==MouseEvent::ButtonLeft || event.button==MouseEvent::ButtonRight)
      skipPhrase();
    return;
    }

  if(event.button==MouseEvent::ButtonLeft) {
    onSelect();
    }
  if(event.button==MouseEvent::ButtonRight) {
    skipPhrase();
    }
  }

void DialogMenu::mouseMoveEvent(MouseEvent& e) {
  // gothic-diablo: the answer under the cursor is the chosen one
  if(Camera::isArpgEnabled() && arpgChoiceReady()) {
    for(size_t i=0; i<choiceRects.size() && i<choice.size(); ++i)
      if(choiceRects[i].contains(e.pos()) && dlgSel!=i) {
        dlgSel = i;
        update();
        break;
        }
    }
  e.ignore();
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
  if(Camera::isArpgEnabled()) {
    // gothic-diablo: 1-9 and 0 pick the answer with that number, Space = the next line
    if(e.key>=Event::K_1 && e.key<=Event::K_9) {
      const size_t id = size_t(e.key-Event::K_1);
      if(isChoiceMenuActive() && id<choice.size()) {
        dlgSel = id;
        onSelect();
        }
      return;
      }
    if(e.key==Event::K_0) {
      if(isChoiceMenuActive() && choice.size()>=10) {
        dlgSel = 9;
        onSelect();
        }
      return;
      }
    if(e.key==Event::K_Space) {
      skipPhrase();
      return;
      }
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

// ---------------------------------------------------------------------------------------------------------------
// gothic-diablo: cinematic dialogs (ARPG mode)

bool DialogMenu::isArpgCinema() const {
  return Camera::isArpgEnabled() && state!=State::Idle && pl!=nullptr && other!=nullptr && pl!=other;
  }

ArpgDialogCam::Input DialogMenu::arpgDialogInput() const {
  ArpgDialogCam::Input in;
  in.pl       = pl;
  in.other    = other;
  in.speaker  = current.time>0 ? (curentIsPl ? pl : other) : nullptr;
  in.line     = lineId;
  in.lineLen  = float(current.msgTime)/1000.f;
  in.third    = (thirdLeft>0) ? third : nullptr;
  if(current.time==0 && thirdLeft>0) {
    in.speaker = third;
    in.lineLen = float(thirdTime)/1000.f;
    }
  in.choosing = isChoiceMenuActive();
  return in;
  }

bool DialogMenu::arpgChoiceReady() const {
  // the list is fully out (a click meant to skip a line does not pick an answer)
  return isChoiceMenuActive() && (!dlgAnimation || choiceAnimTime>=ANIM_TIME);
  }

// a copy of a game font in another colour (cached)
static const GthFont& tintedFont(const GthFont& base, int id, const Color& c) {
  static std::map<std::pair<const GthFont*,int>,GthFont> cache;
  const auto key = std::make_pair(&base, id);
  auto it = cache.find(key);
  if(it==cache.end()) {
    GthFont f = base;
    f.setColor(c);
    it = cache.emplace(key, std::move(f)).first;
    }
  return it->second;
  }

// what kind of answer: trade and teaching get a note
static const char* choiceTag(const GameScript::DlgChoice& c) {
  if(c.isTrade)
    return "(handel)";
  // the titles are in the game's code page: an ASCII search is enough ("Czego mo\xbfesz mnie nauczy\xe6?")
  std::string low;
  low.reserve(c.title.size());
  for(char ch:c.title)
    low.push_back(char(std::tolower(uint8_t(ch))));
  if(low.find("naucz")!=std::string::npos || low.find("nauk")!=std::string::npos)
    return "(nauka)";
  return nullptr;
  }

void DialogMenu::paintArpg(Painter& p) {
  const float k   = std::max(0.5f, float(h())/1080.f);
  const int   bar = std::max(int(float(h())*0.09f), (h()-int(float(w())/2.35f))/2); // 2.35:1 picture
  const float bk  = barsK*barsK*(3.f-2.f*barsK);
  const int   pad = int(16*k);

  auto& fText  = Resources::font("font_old_10_white.tga", Resources::FontType::Hi,       1.40f*k);
  auto& fBase  = Resources::font("font_old_10_white.tga", Resources::FontType::Normal,   1.40f*k);
  auto& fName  = tintedFont(fBase, 0, Color(1.f, 0.80f, 0.42f, 1.f));
  auto& fOpt   = Resources::font("font_old_10_white.tga", Resources::FontType::Hi,       1.30f*k);
  auto& fOptB  = Resources::font("font_old_10_white.tga", Resources::FontType::Normal,   1.30f*k);
  auto& fOptHi = tintedFont(fOptB, 0, Color(1.f, 0.80f, 0.42f, 1.f));
  auto& fOptD  = Resources::font("font_old_10_white.tga", Resources::FontType::Disabled, 1.30f*k);
  auto& fSmall = Resources::font("font_old_10_white.tga", Resources::FontType::Disabled, 1.00f*k);

  auto fill = [&](int x, int y, int ww, int hh, const Color& c) {
    if(ww<=0 || hh<=0)
      return;
    p.setBrush(Brush(c, Painter::Alpha));
    p.drawRect(x, y, ww, hh);
    };

  // the answers: the lower bar grows to fit them
  const bool listOn = isChoiceMenuActive();
  const int  listW  = std::min(int(float(w())*0.56f), int(1050*k));
  const int  listX  = (w()-listW)/2;
  int        listH  = 0;
  if(choice.size()>0) {
    listH = 2*pad;
    for(auto& c:choice)
      listH += std::max(1, int(fOpt.lineCount(listW, c.title)))*fOpt.pixelSize()+int(10*k);
    }
  const float ak   = dlgAnimation ? float(choiceAnimTime)/float(ANIM_TIME) : (listOn ? 1.f : 0.f);
  const int   barH = int(float(bar)*bk);
  int         lowH = barH;
  if(choice.size()>0 && (listOn || choiceAnimTime>0))
    lowH = std::max(lowH, int(float(listH)*ak));

  // the subtitle (not while choosing): name of the one who speaks, the line under it
  std::string_view name;
  int subH = 0, textW = std::min(int(float(w())*0.62f), int(1150*k));
  const bool thirdOn = current.time==0 && thirdLeft>0 && third!=nullptr;
  const std::string& subTxt = thirdOn ? thirdTxt : current.txt;
  const bool subOn = (current.time>0 || thirdOn) && !listOn && trade.isOpen()==InventoryMenu::State::Closed &&
                     haveToShowSubtitles(curentIsPl && !thirdOn);
  if(subOn) {
    if(thirdOn)
      name = third->displayName(); else
      name = curentIsPl ? std::string_view("Bezimienny") : (other!=nullptr ? std::string_view(other->displayName()) : std::string_view());
    const int lines = std::max(1, int(fText.lineCount(textW, subTxt)));
    subH = 2*pad + (name.empty() ? 0 : fName.pixelSize()+int(6*k)) + lines*fText.pixelSize();
    lowH = std::max(lowH, int(float(subH)*std::max(bk, 0.f)));
    }

  // the bars (a soft edge where the lower one is higher than the letterbox)
  fill(0, 0, w(), barH, Color(0.f, 0.f, 0.f, 1.f));
  if(lowH>0) {
    if(lowH>barH+2) {
      const int fade = int(50*k);
      for(int i=0; i<fade; i+=2)
        fill(0, h()-lowH-fade+i, w(), 2, Color(0.f, 0.f, 0.f, float(i)/float(fade)));
      }
    fill(0, h()-lowH, w(), lowH, Color(0.f, 0.f, 0.f, 1.f));
    }

  if(subOn) {
    int y = h()-lowH+(lowH-subH)/2+pad;
    if(!name.empty()) {
      const auto sz = fName.textSize(name);
      fName.drawText(p, (w()-sz.w)/2, y+fName.pixelSize(), name);
      y += fName.pixelSize()+int(6*k);
      }
    fText.drawText(p, (w()-textW)/2, y+fText.pixelSize(), textW, 0, subTxt, AlignHCenter);
    const auto hint = std::string_view("LPM / Spacja - dalej");
    fSmall.drawText(p, w()-fSmall.textSize(hint).w-int(30*k), h()-int(14*k), hint);
    }

  choiceRects.clear();
  if(listOn && ak>=1.f) {
    int y = h()-lowH+pad;
    for(size_t i=0; i<choice.size(); ++i) {
      auto&       c     = choice[i];
      const char* tag   = choiceTag(c);
      const bool  dim   = c.known && tag==nullptr && c.sort!=999; // asked before (the end and services stay bright)
      const bool  sel   = (i==dlgSel);
      auto&       f     = sel ? fOptHi : (dim ? fOptD : fOpt);
      const int   lines = std::max(1, int(f.lineCount(listW, c.title)));
      const int   rh    = lines*f.pixelSize()+int(10*k);

      char num[8] = {};
      std::snprintf(num, sizeof(num), "%d", int((i+1)%10));
      fSmall.drawText(p, listX-int(40*k), y+int(3*k)+fSmall.pixelSize(), num);
      if(sel)
        fill(listX-int(14*k), y+int(4*k), std::max(2, int(4*k)), f.pixelSize()-int(6*k), Color(1.f, 0.80f, 0.42f, 1.f));
      f.drawText(p, listX, y+f.pixelSize(), listW, 0, c.title, AlignLeft);
      if(tag!=nullptr && lines==1)
        fSmall.drawText(p, listX+f.textSize(c.title).w+int(16*k), y+int(3*k)+fSmall.pixelSize(), tag);
      choiceRects.emplace_back(listX-int(50*k), y-int(4*k), listW+int(60*k), rh);
      y += rh;
      }
    const std::string_view hint = "1-9 / mysz - wyb\xf3r"; // the game's code page
    fSmall.drawText(p, w()-fSmall.textSize(hint).w-int(30*k), h()-int(14*k), hint);
    }
  }

// ---------------------------------------------------------------------------------------------------------------
// gothic-diablo: acting in cinematic dialogs

static uint64_t rnd(uint64_t from, uint64_t to) {
  return from + uint64_t(std::rand())%(to-from+1);
  }

void DialogMenu::tickActing(uint64_t dt) {
  if(!isArpgCinema() || state!=State::Active) {
    if(acting.on)
      stopActing();
    return;
    }
  if(!acting.on) {
    acting         = Acting();
    acting.on      = true;
    acting.blinkPl    = rnd(800, 2500);
    acting.blinkOther = rnd(400, 2000);
    }
  acting.t += dt;
  const uint64_t t = acting.t;

  // looks: both keep their eyes on each other (on the third one while he speaks)
  if(third!=nullptr && thirdLeft>0 && current.time==0) {
    pl   ->arpgLookAtHead(third, dt);
    other->arpgLookAtHead(third, dt);
    third->arpgLookAtHead(pl,    dt);
    } else {
    pl   ->arpgLookAtHead(other, dt);
    other->arpgLookAtHead(pl,    dt);
    }

  // blinking: every 2.5 - 5.5 s (close-ups show the faces)
  if(t>=acting.blinkPl) {
    pl->startFaceAnim("R_EYESBLINK", 1.f, uint64_t(-2)); // one blink
    acting.blinkPl = t+rnd(2500, 5500);
    }
  if(t>=acting.blinkOther) {
    other->startFaceAnim("R_EYESBLINK", 1.f, uint64_t(-2));
    acting.blinkOther = t+rnd(2500, 5500);
    }

  // the mood of the other one, from his attitude to the hero (kept fresh while the talk goes on)
  if(t>=acting.moodAt) {
    const char* mood = nullptr; // neutral: the face as it is
    switch(other->world().script().personAttitude(*other, *pl)) {
      case ATT_FRIENDLY: mood = "S_FRIENDLY"; break;
      case ATT_ANGRY:    mood = "S_ANGRY";    break;
      case ATT_HOSTILE:  mood = "S_HOSTILE";  break;
      default:           mood = nullptr;      break;
      }
    if(acting.mood!=nullptr && acting.mood!=mood)
      other->startFaceAnim(acting.mood, 1.f, 0);   // stops the old one
    if(mood!=nullptr)
      other->startFaceAnim(mood, 0.45f, 4000);     // a hint of it, not a mask
    acting.mood   = mood;
    acting.moodAt = t+2000;
    }

  // the listener nods now and then: in the middle of a longer line, not more often than every 6 s
  if(current.time>0 && lineId!=acting.line) {
    acting.line   = lineId;
    acting.nodWho = nullptr;
    Npc* listener = curentIsPl ? other : pl;
    const uint64_t last = (listener==pl) ? acting.lastNodPl : acting.lastNodOther;
    if(current.msgTime>=3500 && t>last+6000 && (std::rand()%3)!=0) {
      acting.nodWho = listener;
      acting.nodAt  = t + current.msgTime*uint64_t(rnd(40, 65))/100u;
      }
    }
  if(acting.nodWho!=nullptr && t>=acting.nodAt) {
    if(current.time>0 && acting.nodWho->arpgNod()) {
      if(acting.nodWho==pl)
        acting.lastNodPl = t; else
        acting.lastNodOther = t;
      }
    acting.nodWho = nullptr;
    }
  }

void DialogMenu::stopActing() {
  if(!acting.on)
    return;
  acting.on = false;
  // straight ahead and a calm face again
  if(pl!=nullptr) {
    pl->arpgLookAtHead(nullptr, 0);
    }
  if(third!=nullptr)
    third->arpgLookAtHead(nullptr, 0);
  if(other!=nullptr && other!=pl) {
    other->arpgLookAtHead(nullptr, 0);
    if(acting.mood!=nullptr)
      other->startFaceAnim(acting.mood, 1.f, 0);
    }
  acting.mood = nullptr;
  }

bool DialogMenu::arpgThirdLine(Npc& npc, std::string_view text, uint64_t ms) {
  // a third one near the two (Lord Hagen's hall, a guard who cuts in): his line becomes part of the film
  if(!isArpgCinema() || &npc==pl || &npc==other || text.empty())
    return false;
  if(npc.qDistTo(*pl)>1500.f*1500.f)
    return false;
  if(third!=nullptr && third!=&npc)
    third->arpgLookAtHead(nullptr, 0);
  third     = &npc;
  thirdTxt  = std::string(text);
  thirdTime = ms;
  thirdLeft = ms+(dlgAnimation ? ANIM_TIME : 0);
  lineId++;
  update();
  return true;
  }
