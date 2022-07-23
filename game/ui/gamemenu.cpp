#include "gamemenu.h"

#include <Tempest/Painter>
#include <Tempest/Log>
#include <Tempest/TextCodec>
#include <Tempest/Dialog>

#include "world/objects/npc.h"
#include "world/world.h"
#include "ui/menuroot.h"
#include "utils/gthfont.h"
#include "utils/fileutil.h"
#include "utils/keycodec.h"
#include "game/definitions/musicdefinitions.h"
#include "game/serialize.h"
#include "game/savegameheader.h"
#include "gothic.h"
#include "resources.h"
#include "build.h"

using namespace Tempest;

static const float scriptDiv=8192.0f;

struct GameMenu::ListContentDialog : Dialog {
  ListContentDialog(Item& textView):textView(textView) {
    setFocusPolicy(ClickFocus);
    setCursorShape(CursorShape::Hidden);
    setFocus(true);
    }

  void mouseDownEvent(MouseEvent& e) override {
    if(e.button==Event::ButtonRight) {
      close();
      }
    }

  void mouseWheelEvent(Tempest::MouseEvent& event) override {
    onMove(-event.delta);
    }

  void keyDownEvent(KeyEvent &e) override { e.accept(); }
  void keyUpEvent  (KeyEvent &e) override {
    if(e.key==Event::K_ESCAPE) {
      close();
      return;
      }
    if(e.key==Event::K_W || e.key==Event::K_Up) {
      onMove(-1);
      update();
      }
    if(e.key==Event::K_S || e.key==Event::K_Down) {
      onMove(+1);
      update();
      }
    }

  void onMove(int dy) {
    if(dy<0) {
      if(textView.scroll>0)
        textView.scroll--;
      }
    if(dy>0) {
      // will be clamped at draw
      textView.scroll++;
      }
    }

  void paintEvent (PaintEvent&) override {}
  void paintShadow(PaintEvent&) override {}

  Item& textView;
  };

struct GameMenu::ListViewDialog : Dialog {
  ListViewDialog(GameMenu& owner, Item& list):owner(owner),list(list){
    setFocusPolicy(ClickFocus);
    setCursorShape(CursorShape::Hidden);
    setFocus(true);
    status = toStatus(list.handle.userString[0]);
    }

  void mouseDownEvent(MouseEvent& e) override {
    if(e.button==Event::ButtonLeft) {
      showQuest();
      }
    if(e.button==Event::ButtonRight) {
      close();
      }
    }

  void showQuest() {
    auto  prev = owner.curItem;
    auto* next = owner.selectedContentItem(&list);
    if(next==nullptr)
      return;

    auto vis = next->visible;
    next->visible = true;
    if(auto ql = selectedQuest()) {
      std::string text;
      for(size_t i=0; i<ql->entry.size(); ++i) {
        text += ql->entry[i];
        if(i+1<ql->entry.size())
          text+="\n---\n";
        }
      next->scroll         = 0;
      next->handle.text[0] = text.c_str();
      }

    for(uint32_t i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i)
      if(&owner.hItems[i]==next) {
        owner.curItem = i;
        break;
        }

    ListContentDialog dlg(*next);
    dlg.resize(owner.owner.size());
    dlg.exec();
    next->visible = vis;
    owner.curItem = prev;
    }

  void keyDownEvent(KeyEvent &e) override { e.accept(); }
  void keyUpEvent  (KeyEvent &e) override {
    if(e.key==Event::K_ESCAPE) {
      close();
      return;
      }
    if(e.key==Event::K_W || e.key==Event::K_Up) {
      onMove(-1);
      update();
      }
    if(e.key==Event::K_S || e.key==Event::K_Down) {
      onMove(+1);
      update();
      }
    }

  void mouseWheelEvent(Tempest::MouseEvent& event) override {
    onMove(-event.delta);
    }

  void onMove(int dy) {
    if(dy<0) {
      if(list.value>0)
        list.value--;
      }
    if(dy>0) {
      const size_t num = numQuests();
      if(list.value+1<int(num))
        list.value++;
      }
    }

  void paintEvent (PaintEvent&) override {}
  void paintShadow(PaintEvent&) override {}

  size_t numQuests() const {
    return size_t(GameMenu::numQuests(Gothic::inst().questLog(),status));
    }

  const QuestLog::Quest* selectedQuest() const {
    int32_t num = 0;
    if(auto ql=Gothic::inst().questLog()) {
      for(size_t i=0; i<ql->questCount(); ++i) {
        auto& quest = ql->quest(ql->questCount()-i-1);
        if(!isCompatible(quest,status))
          continue;
        if(num==list.value)
          return &quest;
        ++num;
        }
      }
    return nullptr;
    }

  GameMenu& owner;
  Item&     list;
  QuestStat status = QuestStat::Log;
  };

struct GameMenu::KeyEditDialog : Dialog {
  KeyEditDialog(){
    setFocusPolicy(ClickFocus);
    setCursorShape(CursorShape::Hidden);
    setFocus(true);
    }
  void keyDownEvent(KeyEvent &e) override { e.accept(); }
  void keyUpEvent  (KeyEvent &e) override {
    close();
    key = e.key;
    }

  void mouseDownEvent(MouseEvent& e) override { e.accept(); }
  void mouseUpEvent  (MouseEvent& e) override {
    close();
    mkey = e.button;
    }

  void paintEvent (PaintEvent&) override {}
  void paintShadow(PaintEvent&) override {}

  Event::KeyType     key  = Event::K_ESCAPE;
  Event::MouseButton mkey = Event::ButtonNone;
  };

struct GameMenu::SavNameDialog : Dialog {
  SavNameDialog(Daedalus::ZString& text):text(text), text0(text) {
    setFocusPolicy(ClickFocus);
    setCursorShape(CursorShape::Hidden);
    setFocus(true);
    }

  void mouseDownEvent(MouseEvent& e) override { e.accept(); }
  void mouseUpEvent  (MouseEvent&) override {
    close();
    accepted = true;
    }

  void keyDownEvent(KeyEvent &e) override { e.accept(); }
  void keyUpEvent  (KeyEvent &e) override {
    if(e.key==Event::K_ESCAPE) {
      text = text0;
      close();
      return;
      }
    if(e.key==Event::K_Return) {
      accepted = true;
      close();
      return;
      }
    if(e.key==Event::K_Back && text.size()>0) {
      std::string tmp = text.c_str();
      tmp.pop_back();
      text = std::move(tmp);
      return;
      }

    char ch[2] = {'\0','\0'};
    if(('a'<=e.code && e.code<='z') || ('A'<=e.code && e.code<='Z')  ||
       ('0'<=e.code && e.code<='9') || e.code==' ' || e.code=='.')
      ch[0] = char(e.code);
    if(ch[0]=='\0')
      return;
    text = text + ch;
    }

  void paintEvent (PaintEvent&) override {}
  void paintShadow(PaintEvent&) override {}

  Daedalus::ZString& text;
  Daedalus::ZString  text0;

  bool                accepted = false;
  };

GameMenu::GameMenu(MenuRoot &owner, KeyCodec& keyCodec, Daedalus::DaedalusVM &vm, const char* menuSection, KeyCodec::Action kClose)
  :owner(owner), keyCodec(keyCodec), vm(vm), kClose(kClose) {
  setCursorShape(CursorShape::Hidden);
  timer.timeout.bind(this,&GameMenu::onTick);
  timer.start(100);

  textBuf.reserve(64);

  Daedalus::DATFile& dat=vm.getDATFile();
  vm.initializeInstance(menu,
                        dat.getSymbolIndexByName(menuSection),
                        Daedalus::IC_Menu);
  back = Resources::loadTexture(menu.backPic.c_str());

  initItems();
  float infoX = 1000.0f/scriptDiv;
  float infoY = 7500.0f/scriptDiv;

  // There could be script-defined values
  if(dat.hasSymbolName("MENU_INFO_X") && dat.hasSymbolName("MENU_INFO_X")) {
    Daedalus::PARSymbol& symX = dat.getSymbolByName("MENU_INFO_X");
    Daedalus::PARSymbol& symY = dat.getSymbolByName("MENU_INFO_Y");

    infoX = float(symX.getInt())/scriptDiv;
    infoY = float(symY.getInt())/scriptDiv;
    }
  setPosition(int(infoX*float(w())),int(infoY*float(h())));

  setSelection(isInGameAndAlive() ? menu.defaultInGame : menu.defaultOutGame);
  updateValues();
  slider = Resources::loadTexture("MENU_SLIDER_POS.TGA");

  up   = Resources::loadTexture("O.TGA");
  down = Resources::loadTexture("U.TGA");

  Gothic::inst().pushPause();
  }

GameMenu::~GameMenu() {
  for(int i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i)
    vm.clearReferences(hItems[i].handle);
  vm.clearReferences(menu);

  Gothic::flushSettings();
  Gothic::inst().popPause();
  Resources::device().waitIdle(); // safe-delete savethumb
  }

GameMenu::QuestStat GameMenu::toStatus(const Daedalus::ZString& str) {
  if(str=="CURRENTMISSIONS")
    return QuestStat::Current;
  if(str=="OLDMISSIONS")
    return QuestStat::Old;
  if(str=="FAILEDMISSIONS")
    return QuestStat::Failed;
  if(str=="LOG")
    return QuestStat::Log;
  return QuestStat::Log;
  }

bool GameMenu::isCompatible(const QuestLog::Quest& q, QuestStat st) {
  if(q.section==QuestLog::Section::Note && st==QuestStat::Log)
    return true;
  return (uint8_t(q.status)==uint8_t(st) && q.section!=QuestLog::Section::Note);
  }

int32_t GameMenu::numQuests(const QuestLog* ql, QuestStat st) {
  if(ql==nullptr)
    return 0;
  int32_t num = 0;
  for(size_t i=0; i<ql->questCount(); ++i)
    if(isCompatible(ql->quest(i),st))
      ++num;
  return num;
  }

void GameMenu::initItems() {
  for(int i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i){
    if(menu.items[i].empty())
      continue;

    hItems[i].name = menu.items[i].c_str();
    vm.initializeInstance(hItems[i].handle,
                          vm.getDATFile().getSymbolIndexByName(hItems[i].name.c_str()),
                          Daedalus::IC_MenuItem);
    hItems[i].img = Resources::loadTexture(hItems[i].handle.backPic.c_str());
    if(hItems[i].handle.type==MENU_ITEM_LISTBOX) {
      hItems[i].visible = false;
      }
    updateItem(hItems[i]);
    }
  }

void GameMenu::paintEvent(PaintEvent &e) {
  Painter p(e);
  p.setScissor(-x(),-y(),owner.w(),owner.h());
  if(back) {
    p.setBrush(*back);
    p.drawRect(0,0,w(),h(),
               0,0,back->w(),back->h());
    }

  for(auto& hItem:hItems)
    drawItem(p,hItem);

  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_SHOW_INFO) {
    if(auto sel=selectedItem()) {
      auto&                                  fnt  = Resources::font();
      Daedalus::GEngineClasses::C_Menu_Item& item = sel->handle;
      if(item.text->size()>0) {
        const char* txt = item.text[1].c_str();
        int tw = fnt.textSize(txt).w;
        fnt.drawText(p,(w()-tw)/2,h()-7,txt);
        }
      }
    }

  if(owner.hasVersionLine()) {
    auto& fnt = Resources::font();
    fnt.drawText(p, w()-fnt.textSize(appBuild).w-25, h()-25, appBuild);
    }
  }

void GameMenu::drawItem(Painter& p, Item& hItem) {
  if(!hItem.visible)
    return;
  Daedalus::GEngineClasses::C_Menu_Item&        item  = hItem.handle;
  Daedalus::GEngineClasses::C_Menu_Item::EFlags flags = Daedalus::GEngineClasses::C_Menu_Item::EFlags(item.flags);
  getText(hItem,textBuf);

  const int32_t dimx = (item.dimx!=-1) ? item.dimx : 8192;
  const int32_t dimy = (item.dimy!=-1) ? item.dimy : 750;

  const int x   = int(float(w()*item.posx)/scriptDiv);
  const int y   = int(float(h()*item.posy)/scriptDiv);
  int       szX = int(float(w()*dimx     )/scriptDiv);
  int       szY = int(float(h()*dimy     )/scriptDiv);

  if(hItem.img && !hItem.img->isEmpty()) {
    p.setBrush(*hItem.img);
    p.drawRect(x,y,szX,szY,
               0,0,hItem.img->w(),hItem.img->h());
    }

  auto& fnt = getTextFont(hItem);

  int tw = szX;
  int th = szY;

  AlignFlag txtAlign=NoAlign;
  if(flags & Daedalus::GEngineClasses::C_Menu_Item::IT_TXT_CENTER) {
    txtAlign = AlignHCenter | AlignVCenter;
    }

  //p.setBrush(Color(1,1,1,1));
  //p.drawRect(x,y,szX,szY);
  {
  int padd = 0;
  if((flags & Daedalus::GEngineClasses::C_Menu_Item::IT_MULTILINE) &&
     std::min(tw,th)>100*2+fnt.pixelSize()) {
    padd = 100; // TODO: find out exact padding formula
    }
  auto tRect   = Rect(x+padd,y+fnt.pixelSize()+padd,
                      tw-2*padd, th-2*padd);

  if(flags & Daedalus::GEngineClasses::C_Menu_Item::IT_MULTILINE) {
    int lineCnt     = fnt.lineCount(tRect.w,textBuf.data());
    int linesInView = tRect.h/fnt.pixelSize();

    hItem.scroll = std::min(hItem.scroll, std::max(lineCnt-linesInView,0));
    hItem.scroll = std::max(hItem.scroll, 0);
    if(lineCnt>linesInView+hItem.scroll) {
      p.setBrush(*down);
      p.drawRect(x+(tw-down->w())/2, y+th-padd,
                 down->w(), down->h());
      }
    if(hItem.scroll>0) {
      p.setBrush(*up);
      p.drawRect(x+(tw-up->w())/2, y+padd-up->h(),
                 up->w(), up->h());
      }
    } else {
    tRect.h = std::max(tRect.h, fnt.pixelSize());
    }

  fnt.drawText(p, tRect.x,tRect.y, tRect.w, tRect.h,
               textBuf.data(), txtAlign, hItem.scroll);
  }

  if(item.type==MENU_ITEM_SLIDER && slider!=nullptr) {
    drawSlider(p,hItem,x,y,szX,szY);
    }
  else if(item.type==MENU_ITEM_LISTBOX) {
    if(auto ql = Gothic::inst().questLog()) {
      const int px = int(float(w()*item.frameSizeX)/scriptDiv);
      const int py = int(float(h()*item.frameSizeY)/scriptDiv);

      auto st = toStatus(item.userString[0]);
      drawQuestList(p, hItem, x+px,y+py, szX-2*px,szY-2*py, *ql,st);
      }
    }
  else if(item.type==MENU_ITEM_INPUT) {
    using namespace Daedalus::GEngineClasses::MenuConstants;
    char textBuf[256]={};

    if(item.onChgSetOptionSection=="KEYS") {
      auto keys = Gothic::settingsGetS(item.onChgSetOptionSection.c_str(), item.onChgSetOption.c_str());
      if(&hItem==ctrlInput)
        std::snprintf(textBuf,sizeof(textBuf),"_"); else
        KeyCodec::keysStr(keys,textBuf,sizeof(textBuf));
      }
    else {
      auto str = item.text[0].c_str();
      if(str[0]=='\0' && &hItem!=ctrlInput && saveSlotId(hItem)!=size_t(-1))
        str = "---";
      if(&hItem==ctrlInput)
        std::snprintf(textBuf,sizeof(textBuf),"%s_",str); else
        std::snprintf(textBuf,sizeof(textBuf),"%s", str);
      }

    fnt.drawText(p,
                 x,y+fnt.pixelSize(),
                 szX, std::max(szY,fnt.pixelSize()),
                 textBuf,
                 txtAlign);
    }
  }

void GameMenu::drawSlider(Painter& p, Item& it, int x, int y, int sw, int sh) {
  float k = float(sh/2)/float(slider->h());
  int w = int(float(slider->w())*k);
  int h = int(float(slider->h())*k);
  p.setBrush(*slider);

  auto& sec = it.handle.onChgSetOptionSection;
  auto& opt = it.handle.onChgSetOption;
  if(sec.empty() || opt.empty())
    return;

  float v  = Gothic::settingsGetF(sec.c_str(),opt.c_str());
  int   dx = int(float(sw-w)*std::max(0.f,std::min(v,1.f)));
  p.drawRect(x+dx,y+(sh-h)/2,w,h,
             0,0,p.brush().w(),p.brush().h());
  }

void GameMenu::drawQuestList(Painter& p, Item& it, int x, int y, int w, int h,
                             const QuestLog& log, QuestStat st) {
  int     itY        = y;
  int32_t listId     = -1;
  int32_t listLen    = numQuests(&log,st);
  int32_t listBegin  = it.scroll;
  int32_t listEnd    = listLen;

  for(size_t i=0; i<log.questCount(); i++) {
    auto& quest = log.quest(log.questCount()-i-1);
    if(!isCompatible(quest,st))
      continue;
    ++listId;
    if(listId<listBegin)
      continue;

    auto ft = Resources::FontType::Normal;
    if(listId==it.value && selectedItem()==&it)
      ft = Resources::FontType::Hi;

    auto& fnt = Resources::font(it.handle.fontName.c_str(),ft);
    auto  sz  = fnt.textSize(w,quest.name);
    if(itY+sz.h>h+y) {
      listEnd = listId;
      break;
      }

    fnt.drawText(p,x,itY+int(fnt.pixelSize()),w,sz.h,quest.name,Tempest::AlignLeft);
    itY+=sz.h;
    }

  if(listBegin>it.value) {
    it.scroll = std::max(it.scroll-1,0);
    update();
    }
  if(listEnd<=it.value) {
    it.scroll = std::min(it.scroll+1,listLen);
    update();
    }

  if(listEnd<listLen && down!=nullptr) {
    p.setBrush(*down);
    p.drawRect(x+w-down->w(),y+h-down->h(),
               down->w(),down->h());
    }
  if(listBegin>0 && up!=nullptr) {
    p.setBrush(*up);
    p.drawRect(x+w-up->w(),y,
               up->w(),up->h());
    }
  }

void GameMenu::resizeEvent(SizeEvent &) {
  onTick();
  }

void GameMenu::onMove(int dy) {
  Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_BROWSE"));
  setSelection(int(curItem)+dy, dy>0 ? 1 : -1);
  if(auto s = selectedItem())
    updateSavThumb(*s);
  update();
  }

void GameMenu::onSelect() {
  if(auto sel=selectedItem()){
    Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_SELECT"));
    exec(*sel,0);
    }
  }

void GameMenu::onSlide(int dx) {
  if(auto sel=selectedItem()){
    exec(*sel,dx);
    }
  }

void GameMenu::onTick() {
  update();

  const float fx = 640.0f;
  const float fy = 480.0f;

  const float wx = float(owner.w());
  const float wy = float(owner.h());

  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_DONTSCALE_DIM)
    resize(int(float(menu.dimx)/scriptDiv*fx),int(float(menu.dimy)/scriptDiv*fy)); else
    resize(int(float(menu.dimx)/scriptDiv*wx),int(float(menu.dimy)/scriptDiv*wy));

  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_ALIGN_CENTER) {
    setPosition((owner.w()-w())/2, (owner.h()-h())/2);
    } else {
    setPosition(int(float(menu.posx)/scriptDiv*fx), int(float(menu.posy)/scriptDiv*fy));
    }
  }

void GameMenu::processMusicTheme() {
  if(auto theme = Gothic::musicDef()[menu.musicTheme.c_str()])
    GameMusic::inst().setMusic(*theme,GameMusic::mkTags(GameMusic::Std,GameMusic::Day));
  }

GameMenu::Item *GameMenu::selectedItem() {
  if(curItem<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS)
    return &hItems[curItem];
  return nullptr;
  }

GameMenu::Item* GameMenu::selectedNextItem(Item *it) {
  uint32_t cur=curItem+1;
  for(uint32_t i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i)
    if(&hItems[i]==it) {
      cur=i+1;
      break;
      }

  for(int i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i,cur++) {
    cur%=Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;

    auto& it=hItems[cur].handle;
    if(isEnabled(it))
      return &hItems[cur];
    }
  return nullptr;
  }

GameMenu::Item* GameMenu::selectedContentItem(Item *it) {
  uint32_t cur=curItem+1;
  for(uint32_t i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i)
    if(&hItems[i]==it) {
      cur=i+1;
      break;
      }

  for(int i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i,cur++) {
    cur%=Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;

    auto& it=hItems[cur].handle;
    if(isEnabled(it) && it.type==MENU_ITEM_TEXT)
      return &hItems[cur];
    }
  return nullptr;
  }

void GameMenu::setSelection(int desired, int seek) {
  uint32_t cur=uint32_t(desired);
  for(int i=0; i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS; ++i,cur+=uint32_t(seek)) {
    cur%=Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;

    auto& it=hItems[cur].handle;
    if(isSelectable(it) && isEnabled(it)){
      curItem=cur;
      for(size_t i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_SEL_ACTIONS;++i)
        if(it.onSelAction[i]==Daedalus::GEngineClasses::MenuConstants::SEL_ACTION_EXECCOMMANDS)
          execCommands(it.onSelAction_S[i],false);
      return;
      }
    }
  curItem=uint32_t(-1);
  }

void GameMenu::getText(const Item& it, std::vector<char> &out) {
  if(out.size()==0)
    out.resize(1);
  out[0]='\0';

  const auto& src = it.handle.text[0];
  if(it.handle.type==Daedalus::GEngineClasses::C_Menu_Item::MENU_ITEM_TEXT) {
    size_t size = std::strlen(src.c_str());
    out.resize(size+1);
    std::memcpy(out.data(),src.c_str(),size+1);
    return;
    }

  if(it.handle.type==Daedalus::GEngineClasses::C_Menu_Item::MENU_ITEM_CHOICEBOX) {
    strEnum(src.c_str(),it.value,out);
    return;
    }
  }

const GthFont& GameMenu::getTextFont(const GameMenu::Item &it) {
  if(!isEnabled(it.handle))
    return Resources::font(it.handle.fontName.c_str(),Resources::FontType::Disabled);
  if(&it==selectedItem())
    return Resources::font(it.handle.fontName.c_str(),Resources::FontType::Hi);
  return Resources::font(it.handle.fontName.c_str());
  }

bool GameMenu::isSelectable(const Daedalus::GEngineClasses::C_Menu_Item& item) {
  return (item.flags & Daedalus::GEngineClasses::C_Menu_Item::EFlags::IT_SELECTABLE);
  }

bool GameMenu::isEnabled(const Daedalus::GEngineClasses::C_Menu_Item &item) {
  if((item.flags & Daedalus::GEngineClasses::C_Menu_Item::IT_ONLY_IN_GAME) && !isInGameAndAlive())
    return false;
  if((item.flags & Daedalus::GEngineClasses::C_Menu_Item::IT_ONLY_OUT_GAME) && isInGameAndAlive())
    return false;
  return true;
  }

void GameMenu::exec(Item &p, int slideDx) {
  auto* it = &p;
  while(it!=nullptr){
    if(it==&p)
      execSingle(*it,slideDx); else
      execChgOption(*it,slideDx);
    if(it->handle.flags & Daedalus::GEngineClasses::C_Menu_Item::IT_EFFECTS_NEXT) {
      auto next=selectedNextItem(it);
      if(next!=&p)
        it=next;
      } else {
      it=nullptr;
      }
    }

  if(closeFlag)
    owner.closeAll();
  else if(exitFlag)
    owner.popMenu();
  }

void GameMenu::execSingle(Item &it, int slideDx) {
  using namespace Daedalus::GEngineClasses::MenuConstants;

  auto& item          = it.handle;
  auto& onSelAction   = item.onSelAction;
  auto& onSelAction_S = item.onSelAction_S;
  auto& onEventAction = item.onEventAction;

  if(item.type==Daedalus::GEngineClasses::C_Menu_Item::MENU_ITEM_INPUT && slideDx==0) {
    ctrlInput = &it;
    if(item.onChgSetOption.empty()) {
      SavNameDialog dlg{item.text[0]};
      dlg.resize(owner.size());
      dlg.exec();
      ctrlInput = nullptr;
      if(!dlg.accepted)
        return;
      } else {
      KeyEditDialog dlg;
      dlg.resize(owner.size());
      dlg.exec();
      ctrlInput = nullptr;

      if(dlg.key==Event::K_ESCAPE && dlg.mkey==Event::ButtonNone)
        return;
      int32_t next = 0;
      if(dlg.key==Event::K_ESCAPE)
        next = KeyCodec::keyToCode(dlg.mkey); else
        next = KeyCodec::keyToCode(dlg.key);
      keyCodec.set(item.onChgSetOptionSection.c_str(), item.onChgSetOption.c_str(), next);
      updateItem(it);
      return; //HACK: there is a SEL_ACTION_BACK token in same item
      }
    }

  for(size_t i=0; i<Daedalus::GEngineClasses::MenuConstants::MAX_SEL_ACTIONS; ++i) {
    auto action = onSelAction[i];
    switch(action) {
      case SEL_ACTION_UNDEF:
        break;
      case SEL_ACTION_BACK:
        Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_ESC"));
        exitFlag = true;
        break;
      case SEL_ACTION_STARTMENU:
        if(vm.getDATFile().hasSymbolName(onSelAction_S[i].c_str()))
          owner.pushMenu(new GameMenu(owner,keyCodec,vm,onSelAction_S[i].c_str(),keyClose()));
        break;
      case SEL_ACTION_STARTITEM:
        break;
      case SEL_ACTION_CLOSE:
        Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_ESC"));
        closeFlag = true;
        if(onSelAction_S[i]=="NEW_GAME")
          Gothic::inst().onStartGame(Gothic::inst().defaultWorld());
        else if(onSelAction_S[i]=="LEAVE_GAME")
          Tempest::SystemApi::exit();
        else if(onSelAction_S[i]=="SAVEGAME_SAVE")
          execSaveGame(it);
        else if(onSelAction_S[i]=="SAVEGAME_LOAD")
          execLoadGame(it);
        break;
      case SEL_ACTION_CONCOMMANDS:
        // unknown
        break;
      case SEL_ACTION_PLAY_SOUND:
        Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx(onSelAction_S[i].c_str()));
        break;
      case SEL_ACTION_EXECCOMMANDS:
        execCommands(onSelAction_S[i],true);
        break;
      }
    }

  if(onEventAction[SEL_EVENT_EXECUTE]>0){
    vm.runFunctionBySymIndex(size_t(onEventAction[SEL_EVENT_EXECUTE]));
    }

  execChgOption(it,slideDx);
  }

void GameMenu::execChgOption(Item &item, int slideDx) {
  auto& sec = item.handle.onChgSetOptionSection;
  auto& opt = item.handle.onChgSetOption;
  if(sec.empty() || opt.empty())
    return;

  if(item.handle.type==Daedalus::GEngineClasses::C_Menu_Item::MENU_ITEM_SLIDER && slideDx!=0) {
    updateItem(item);
    float v = Gothic::settingsGetF(sec.c_str(),opt.c_str());
    v  = std::max(0.f,std::min(v+float(slideDx)*0.03f,1.f));
    Gothic::settingsSetF(sec.c_str(), opt.c_str(), v);
    }
  if(item.handle.type==Daedalus::GEngineClasses::C_Menu_Item::MENU_ITEM_CHOICEBOX && slideDx==0) {
    updateItem(item);
    item.value += 1; // next value

    int cnt = int(strEnumSize(item.handle.text[0].c_str()));
    if(cnt>0)
      item.value%=cnt; else
      item.value =0;
    Gothic::settingsSetI(sec.c_str(), opt.c_str(), item.value);
    }
  }

void GameMenu::execSaveGame(GameMenu::Item &item) {
  const size_t id = saveSlotId(item);
  if(id==size_t(-1))
    return;

  char fname[64]={};
  std::snprintf(fname,sizeof(fname)-1,"save_slot_%d.sav",int(id));
  Gothic::inst().save(fname,item.handle.text[0].c_str());
  }

void GameMenu::execLoadGame(GameMenu::Item &item) {
  const size_t id = saveSlotId(item);
  if(id==size_t(-1))
    return;

  char fname[64]={};
  std::snprintf(fname,sizeof(fname)-1,"save_slot_%d.sav",int(id));
  Gothic::inst().load(fname);
  }

void GameMenu::execCommands(const Daedalus::ZString str, bool isClick) {
  using namespace Daedalus::GEngineClasses::MenuConstants;

  if(str.find("EFFECTS ")==0) {
    // menu log
    const char* arg0 = str.c_str()+std::strlen("EFFECTS ");
    for(uint32_t id=0; id<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS; ++id) {
      auto& i = hItems[id];
      if(i.handle.type==MENU_ITEM_LISTBOX) {
        i.visible = (i.name==arg0);
        if(i.visible && isClick) {
          const uint32_t prev = curItem;
          curItem = id;
          ListViewDialog dlg(*this,i);
          dlg.resize(owner.size());
          dlg.exec();
          curItem = prev;
          }
        }
      }
    }

  if(!isClick)
    return;
  if(str.find("RUN ")==0) {
    const char* what = str.c_str()+4;
    while(*what==' ')
      ++what;
    for(auto& i:hItems)
      if(i.name==what)
        execSingle(i,0);
    }
  if(str=="SETDEFAULT") {
    setDefaultKeys("KEYSDEFAULT0");
    }
  if(str=="SETALTERNATIVE") {
    setDefaultKeys("KEYSDEFAULT1");
    }
  }

void GameMenu::updateItem(GameMenu::Item &item) {
  auto& it   = item.handle;
  item.value = Gothic::settingsGetI(it.onChgSetOptionSection.c_str(), it.onChgSetOption.c_str());
  updateSavTitle(item);
  }

void GameMenu::updateSavTitle(GameMenu::Item& sel) {
  if(sel.handle.onSelAction_S[0]!="SAVEGAME_LOAD" &&
     sel.handle.onSelAction_S[1]!="SAVEGAME_SAVE")
    return;
  const size_t id = saveSlotId(sel);
  if(id==size_t(-1))
    return;

  char fname[64]={};
  std::snprintf(fname,sizeof(fname)-1,"save_slot_%d.sav",int(id));

  if(!FileUtil::exists(TextCodec::toUtf16(fname))) {
    sel.handle.text[0] = "---";
    return;
    }

  SaveGameHeader hdr;
  try {
    RFile     fin(fname);
    Serialize reader(fin);
    reader.setEntry("header");
    reader.read(hdr);
    sel.handle.text[0] = hdr.name.c_str();
    sel.savHdr         = std::move(hdr);

    if(reader.setEntry("priview.png"))
      reader.read(sel.savPriview);
    }
  catch(std::bad_alloc&) {
    return;
    }
  catch(std::system_error& e) {
    Log::d(e.what());
    return;
    }
  catch(std::runtime_error&) {
    return;
    }
  }

void GameMenu::updateSavThumb(GameMenu::Item &sel) {
  if(sel.handle.onSelAction_S[0]!="SAVEGAME_LOAD" &&
     sel.handle.onSelAction_S[1]!="SAVEGAME_SAVE")
    return;

  if(!implUpdateSavThumb(sel)) {
    set("MENUITEM_LOADSAVE_THUMBPIC",static_cast<Texture2d*>(nullptr));
    set("MENUITEM_LOADSAVE_LEVELNAME_VALUE","");
    set("MENUITEM_LOADSAVE_DATETIME_VALUE", "");
    set("MENUITEM_LOADSAVE_GAMETIME_VALUE", "");
    set("MENUITEM_LOADSAVE_PLAYTIME_VALUE", "");
    }
  }

void GameMenu::updateVideo() {
  set("MENUITEM_VID_DEVICE_CHOICE",     Resources::renderer());
  set("MENUITEM_VID_RESOLUTION_CHOICE", "");
  }

void GameMenu::setDefaultKeys(const char* preset) {
  keyCodec.setDefaultKeys(preset);
  }

bool GameMenu::isInGameAndAlive() {
  auto pl = Gothic::inst().player();
  if(pl==nullptr || pl->isDead())
    return false;
  return Gothic::inst().isInGame();
  }

bool GameMenu::implUpdateSavThumb(GameMenu::Item& sel) {
  const size_t id = saveSlotId(sel);
  if(id==size_t(-1))
    return false;

  char fname[64]={};
  std::snprintf(fname,sizeof(fname)-1,"save_slot_%d.sav",int(id));

  if(!FileUtil::exists(TextCodec::toUtf16(fname)))
    return false;

  const SaveGameHeader& hdr = sel.savHdr;
  char form[64]={};
  Resources::device().waitIdle();
  savThumb = Resources::loadTexturePm(sel.savPriview);

  set("MENUITEM_LOADSAVE_THUMBPIC",       &savThumb);
  set("MENUITEM_LOADSAVE_LEVELNAME_VALUE",hdr.world.c_str());

  std::snprintf(form,sizeof(form),"%d.%d.%d - %d:%02d",int(hdr.pcTime.tm_mday),int(hdr.pcTime.tm_mon+1),int(1900+hdr.pcTime.tm_year), int(hdr.pcTime.tm_hour),int(hdr.pcTime.tm_min));
  set("MENUITEM_LOADSAVE_DATETIME_VALUE", form);

  std::snprintf(form,sizeof(form),"%d - %d:%02d",int(hdr.wrldTime.day()),int(hdr.wrldTime.hour()),int(hdr.wrldTime.minute()));
  set("MENUITEM_LOADSAVE_GAMETIME_VALUE", form);

  auto mins = hdr.playTime/(60*1000);
  std::snprintf(form,sizeof(form),"%dh %dmin",int(mins/60),int(mins%60));
  set("MENUITEM_LOADSAVE_PLAYTIME_VALUE", form);
  return true;
  }

size_t GameMenu::saveSlotId(const GameMenu::Item &sel) {
  const char* prefix[2] = {"MENUITEM_LOAD_SLOT", "MENUITEM_SAVE_SLOT"};
  for(auto p:prefix) {
    if(sel.name.find(p)==0){
      size_t off=std::strlen(p);
      size_t id=0;
      for(size_t i=off;i<sel.name.size();++i){
        if('0'<=sel.name[i] && sel.name[i]<='9') {
          id=id*10+size_t(sel.name[i]-'0');
          } else {
          return size_t(-1);
          }
        }
      return id;
      }
    }
  return size_t(-1);
  }

const char *GameMenu::strEnum(const char *en, int id, std::vector<char> &out) {
  int num=0;

  size_t i=0;
  for(size_t r=0;en[r];++r)
    if(en[r]=='#'){
      i=r+1;
      break;
      }

  for(;en[i];++i){
    size_t b=i;
    for(size_t r=i;en[r];++r,++i)
      if(en[r]=='|')
        break;

    if(id==num) {
      size_t sz=i-b;
      out.resize(sz+1);
      std::memcpy(&out[0],&en[b],sz);
      out[sz]='\0';
      return textBuf.data();
      }
    ++num;
    }
  for(size_t i=0;en[i];++i){
    if(en[i]=='#' || en[i]=='|'){
      out.resize(i+1);
      std::memcpy(&out[0],en,i);
      out[i]='\0';
      return out.data();
      }
    }
  return "";
  }

size_t GameMenu::strEnumSize(const char *en) {
  if(en==nullptr)
    return 0;
  size_t cnt = 0;
  for(size_t i=0;en[i];++i) {
    if(en[i]=='#' || en[i]=='|') {
      cnt += en[i]=='|' ? 2 : 1;
      i++;
      for(;en[i];++i) {
        if(en[i]=='|')
          cnt++;
        }
      }
    }
  return cnt;
  }

void GameMenu::set(std::string_view item, const Texture2d *value) {
  for(auto& i:hItems)
    if(i.name==item) {
      i.img = value;
      return;
      }
  }

void GameMenu::set(std::string_view item, const uint32_t value) {
  char buf[16]={};
  std::snprintf(buf,sizeof(buf),"%u",value);
  set(item,buf);
  }

void GameMenu::set(std::string_view item, const int32_t value) {
  char buf[16]={};
  std::snprintf(buf,sizeof(buf),"%d",value);
  set(item,buf);
  }

void GameMenu::set(std::string_view item, const int32_t value, const char *post) {
  char buf[32]={};
  std::snprintf(buf,sizeof(buf),"%d%s",value,post);
  set(item,buf);
  }

void GameMenu::set(std::string_view item, const int32_t value, const int32_t max) {
  char buf[32]={};
  std::snprintf(buf,sizeof(buf),"%d/%d",value,max);
  set(item,buf);
  }

void GameMenu::set(std::string_view item, const char* value) {
  for(auto& i:hItems)
    if(i.name==item) {
      i.handle.text[0] = value;
      return;
      }
  }

void GameMenu::updateValues() {
  gtime time;
  if(auto w = Gothic::inst().world())
    time = w->time();

  set("MENU_ITEM_PLAYERGUILD","Debugger");
  set("MENU_ITEM_LEVEL","0");
  for(auto& i:hItems) {
    if(i.name=="MENU_ITEM_CONTENT_VIEWER") {
      i.visible=false;
      }
    if(i.name=="MENU_ITEM_DAY") {
      i.handle.text[0] = Daedalus::ZString::toStr(time.day());
      }
    if(i.name=="MENU_ITEM_TIME") {
      char form[64]={};
      std::snprintf(form,sizeof(form),"%d:%02d",int(time.hour()),int(time.minute()));
      i.handle.text[0] = form;
      }
    }

  if(auto s = selectedItem())
    updateSavThumb(*s);

  updateVideo();
  }

void GameMenu::setPlayer(const Npc &pl) {
  auto world = Gothic::inst().world();
  if(world==nullptr)
    return;

  auto& sc    = world->script();
  auto& gilds = sc.getSymbol("TXT_GUILDS");
  auto& tal   = sc.getSymbol("TXT_TALENTS");
  auto& talV  = sc.getSymbol("TXT_TALENTS_SKILLS");

  set("MENU_ITEM_PLAYERGUILD",gilds.getString(pl.guild()).c_str());

  set("MENU_ITEM_LEVEL",      pl.level());
  set("MENU_ITEM_EXP",        pl.experience());
  set("MENU_ITEM_LEVEL_NEXT", pl.experienceNext());
  set("MENU_ITEM_LEARN",      pl.learningPoints());

  set("MENU_ITEM_ATTRIBUTE_1", pl.attribute(ATR_STRENGTH));
  set("MENU_ITEM_ATTRIBUTE_2", pl.attribute(ATR_DEXTERITY));
  set("MENU_ITEM_ATTRIBUTE_3", pl.attribute(ATR_MANA),      pl.attribute(ATR_MANAMAX));
  set("MENU_ITEM_ATTRIBUTE_4", pl.attribute(ATR_HITPOINTS), pl.attribute(ATR_HITPOINTSMAX));

  set("MENU_ITEM_ARMOR_1", pl.protection(PROT_EDGE));
  set("MENU_ITEM_ARMOR_2", pl.protection(PROT_POINT)); // not sure about it
  set("MENU_ITEM_ARMOR_3", pl.protection(PROT_FIRE));
  set("MENU_ITEM_ARMOR_4", pl.protection(PROT_MAGIC));

  const int talentMax = Gothic::inst().version().game==2 ? TALENT_MAX_G2 : TALENT_MAX_G1;
  for(int i=0;i<talentMax;++i){
    auto& str = tal.getString(size_t(i));
    if(str.empty())
      continue;

    char buf[64]={};
    std::snprintf(buf,sizeof(buf),"MENU_ITEM_TALENT_%d_TITLE",i);
    set(buf,str.c_str());

    const int sk=pl.talentSkill(Talent(i));
    std::snprintf(buf,sizeof(buf),"MENU_ITEM_TALENT_%d_SKILL",i);
    set(buf,strEnum(talV.getString(size_t(i)).c_str(),sk,textBuf));

    const int val=pl.hitChanse(Talent(i));
    std::snprintf(buf,sizeof(buf),"MENU_ITEM_TALENT_%d",i);
    set(buf,val,"%");
    }
  }
