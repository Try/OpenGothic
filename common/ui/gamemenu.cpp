#include "gamemenu.h"

#include <Tempest/Painter>
#include <Tempest/Log>
#include <Tempest/TextCodec>
#include <Tempest/Dialog>
#include <Tempest/Application>

#include <cmath>

#include <algorithm>

#include "utils/string_frm.h"
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
#include "game/arpgkeys.h"
#include "ui/arpgmenu.h"
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
      }
    if(e.key==Event::K_S || e.key==Event::K_Down) {
      onMove(+1);
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
    status = toStatus(list.handle->user_string[0]);
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
    auto* ql   = selectedQuest();
    if(next==nullptr || ql==nullptr)
      return;

    auto vis = next->visible;
    next->visible = true;

    std::string text;
    for(size_t i=0; i<ql->entry.size(); ++i) {
      text += ql->entry[i];
      if(i+1<ql->entry.size())
        text+="\n---\n";
      }
    next->scroll          = 0;
    next->handle->text[0] = text;

    for(uint32_t i=0; i<zenkit::IMenu::item_count; ++i)
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

  void keyRepeatEvent(KeyEvent &e) override {
    keyUpEvent(e);
    }

  void keyUpEvent  (KeyEvent &e) override {
    if(e.key==Event::K_Return) {
      showQuest();
      return;
      }
    if(e.key==Event::K_ESCAPE) {
      close();
      return;
      }
    if(e.key==Event::K_W || e.key==Event::K_Up) {
      onMove(-1);
      }
    if(e.key==Event::K_S || e.key==Event::K_Down) {
      onMove(+1);
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
    update();
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
  SavNameDialog(std::string& text):text(text), text0(text) {
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
    update();

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
      text.pop_back();
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

  std::string& text;
  std::string  text0;

  bool         accepted = false;
  };

GameMenu::GameMenu(MenuRoot &owner, KeyCodec& keyCodec, zenkit::DaedalusVm& vm, std::string_view menuSection, KeyCodec::Action kClose)
  :owner(owner), keyCodec(keyCodec), vm(&vm), kClose(kClose) {
  setCursorShape(CursorShape::Hidden);
  timer.timeout.bind(this,&GameMenu::onTick);
  timer.start(100);

  textBuf.reserve(64);

  auto* menuSectionSymbol = vm.find_symbol_by_name(menuSection);
  if(menuSectionSymbol!=nullptr) {
    menu = vm.init_instance<zenkit::IMenu>(menuSectionSymbol);
    } else {
    Tempest::Log::e("Cannot initialize menu ", menuSection, ": Symbol not found.");
    menu = std::make_shared<zenkit::IMenu>();
    }

  back = Resources::loadTexture(menu->back_pic);

  // Diathic: the main menu gets the modern layout
  modern = (menuSection.size()==9);
  for(size_t i=0; modern && i<menuSection.size(); ++i)
    modern = (std::toupper(uint8_t(menuSection[i]))==uint8_t("MENU_MAIN"[i]));

  initItems();
  float infoX = 1000.0f/scriptDiv;
  float infoY = 7500.0f/scriptDiv;

  // There could be script-defined values
  if(vm.find_symbol_by_name("MENU_INFO_X") != nullptr && vm.find_symbol_by_name("MENU_INFO_X") != nullptr) {
    auto* symX = vm.find_symbol_by_name("MENU_INFO_X");
    auto* symY = vm.find_symbol_by_name("MENU_INFO_Y");

    infoX = float(symX->get_int())/scriptDiv;
    infoY = float(symY->get_int())/scriptDiv;
    }
  setPosition(int(infoX*float(w())),int(infoY*float(h())));

  setSelection(Gothic::inst().isInGameAndAlive() ? menu->default_ingame : menu->default_outgame);
  updateValues();
  slider = Resources::loadTexture("MENU_SLIDER_POS.TGA");

  up     = Resources::loadTexture("O.TGA");
  down   = Resources::loadTexture("U.TGA");

  Gothic::inst().pushPause();
  }

GameMenu::~GameMenu() {
  Gothic::flushSettings();
  Gothic::inst().popPause();
  Resources::device().waitIdle(); // safe-delete savethumb
  }

void GameMenu::resetVm(zenkit::DaedalusVm* inVm) {
  vm = inVm;
  for(int i=0; i<zenkit::IMenu::item_count; ++i){
    hItems[i].handle = nullptr;
    }
  if(vm==nullptr)
    return;
  initItems();
  updateValues();
  }

GameMenu::QuestStat GameMenu::toStatus(std::string_view str) {
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
  for(int i=0; i<zenkit::IMenu::item_count; ++i){
    if(menu->items[i].empty())
      continue;

    hItems[i].name = menu->items[i];

    auto* menuItemSymbol = vm->find_symbol_by_name(hItems[i].name);
    if (menuItemSymbol != nullptr) {
      hItems[i].handle = vm->init_instance<zenkit::IMenuItem>(menuItemSymbol);
      } else {
      Tempest::Log::e("Cannot initialize menu item ", hItems[i].name, ": Symbol not found.");
      hItems[i].handle = std::make_shared<zenkit::IMenuItem>();
      }

    hItems[i].img = Resources::loadTexture(hItems[i].handle->backpic);

    if(hItems[i].handle->type==zenkit::MenuItemType::LISTBOX) {
      hItems[i].visible = false;
      }
    updateItem(hItems[i]);
    }
  }

void GameMenu::paintEvent(PaintEvent &e) {
  const float scale = Gothic::interfaceScale(this);

  Painter p(e);
  p.setScissor(-x(),-y(),owner.w(),owner.h());

  if(modern) {
    paintModern(p);
    return;
    }

  if(back) {
    p.setBrush(*back);
    p.drawRect(0,0,w(),h(),
               0,0,back->w(),back->h());
    }

  for(auto& hItem:hItems)
    drawItem(p,hItem);

  if(menu->flags & zenkit::MenuFlag::SHOW_INFO) {
    if(auto sel=selectedItem()) {
      auto& item = sel->handle;
      if(item->text->size()>0) {
        auto             fnt   = Resources::font(scale);
        std::string_view txt   = item->text[1];
        int              tw    = fnt.textSize(txt).w;

        fnt.drawText(p,(w()-tw)/2,h()-int(7*scale),txt);
        }
      }
    }

  if(owner.hasVersionLine()) {
    auto&       fnt   = Resources::font(scale);
    fnt.drawText(p, w()-fnt.textSize(appBuild).w-int(25*scale), h()-int(25*scale), appBuild);
    }
  }

// Diathic: the items of the modern main menu from top to bottom; -2 = "Sterowanie" (ours, after the options)
std::vector<int> GameMenu::modernOrder() {
  std::vector<int> ret;
  for(int i=0; i<zenkit::IMenu::item_count; ++i) {
    auto& it = hItems[i];
    if(!it.visible || it.name.empty() || it.handle==nullptr || isHidden(it.handle))
      continue;
    if(!isSelectable(it.handle) || !isEnabled(it.handle))
      continue;
    ret.push_back(i);
    }
  std::stable_sort(ret.begin(), ret.end(), [this](int a, int b){ return hItems[a].handle->pos_y<hItems[b].handle->pos_y; });
  size_t at = ret.size();
  for(size_t i=0; i<ret.size(); ++i) {
    if(hItems[ret[i]].name=="MENUITEM_MAIN_OPTIONS")
      at = i+1;
    if(hItems[ret[i]].name=="MENUITEM_MAIN_EXIT" && at==ret.size())
      at = i;
    }
  ret.insert(ret.begin()+std::ptrdiff_t(at), -2);
  return ret;
  }

void GameMenu::paintModern(Painter& p) {
  const int   W = w(), H = h();
  const float k = float(H)/1080.f;
  const bool  inGame = Gothic::inst().isInGame();

  // in the game: the game darkened behind the menu
  if(inGame) {
    p.setBrush(Brush(Color(0.f,0.f,0.f,0.45f), Painter::Alpha));
    p.drawRect(0, 0, W, H);
    }
  // a dark band on the left, fading to the right
  {
    const int panelW = int(float(W)*(modernPage==1 ? 0.62f : 0.48f));
    const int steps  = 32;
    for(int i=0; i<steps; ++i) {
      const float t = float(i)/float(steps);
      const float a = 0.82f*std::pow(1.f-t, 1.6f);
      p.setBrush(Brush(Color(0.02f,0.01f,0.01f,a), Painter::Alpha));
      p.drawRect(int(float(panelW)*t), 0, panelW/steps+1, H);
      }
  }

  // fonts: the old Gothic font, about 36 px at 1080 lines
  const float base  = float(std::max(1, Resources::font(1.f).pixelSize()));
  GthFont     fItem = Resources::font(Resources::FontType::Normal, 36.f*k/base);
  GthFont     fSel  = Resources::font(Resources::FontType::Hi,     36.f*k/base);
  GthFont     fInfo = Resources::font(Resources::FontType::Normal, 22.f*k/base);
  fItem.setColor(Color(0.90f, 0.85f, 0.76f, 1.f));
  fSel .setColor(Color(1.00f, 0.82f, 0.45f, 1.f));
  fInfo.setColor(Color(0.78f, 0.70f, 0.58f, 1.f));
  const uint64_t now = Application::tickCount();

  auto band = [&](const Rect& r) {
    // the selected line: a glowing band fading to the right, a bright edge on the left
    const int   steps = 20;
    const float pulse = 0.85f + 0.15f*std::sin(float(now%100000)*0.004f);
    for(int i=0; i<steps; ++i) {
      const float t = float(i)/float(steps);
      p.setBrush(Brush(Color(0.75f,0.22f,0.06f, 0.42f*(1.f-t)*pulse), Painter::Alpha));
      p.drawRect(r.x+int(float(r.w)*t), r.y+int(4.f*k), r.w/steps+1, r.h-int(8.f*k));
      }
    p.setBrush(Brush(Color(1.f,0.72f,0.30f,1.f), Painter::Alpha));
    p.drawRect(r.x, r.y+int(4.f*k), std::max(2,int(4.f*k)), r.h-int(8.f*k));
    };

  for(auto& it:hItems)
    it.modernRect = Rect();
  keysItemRect = Rect();
  keysRows.clear();

  if(modernPage==1) {
    paintModernKeys(p, fItem, fSel, fInfo, band);
    } else {
    // the logo (MENU_GOTHIC.TGA = the Diathic logo), top left
    int lx = int(float(W)*0.05f), ly = int(float(H)*0.05f), lw = 0, lh = 0;
    if(auto logo = Resources::loadTexture("MENU_GOTHIC.TGA")) {
      const float aspect = float(logo->w())/float(std::max(1,logo->h()));
      lw = int(float(W)*0.32f);
      lh = int(float(lw)/aspect);
      if(lh>int(float(H)*0.30f)) {
        lh = int(float(H)*0.30f);
        lw = int(float(lh)*aspect);
        }
      p.setBrush(Brush(*logo, Painter::Alpha));
      p.drawRect(lx, ly, lw, lh, 0, 0, logo->w(), logo->h());
      }
    {
      const std::string_view sub = "Gothic II  -  Noc Kruka";
      const auto ts = fInfo.textSize(sub);
      fInfo.drawText(p, lx+(lw-ts.w)/2, ly+lh+int(6.f*k), sub);
    }

    const int x0    = lx + int(18.f*k);
    const int itemH = int(56.f*k);
    const int itemW = int(float(W)*0.30f);
    int       y     = std::max(ly+lh+int(70.f*k), int(float(H)*0.40f));
    const auto sel  = selectedItem();
    for(int idx:modernOrder()) {
      const Rect r(x0-int(14.f*k), y, itemW, itemH);
      bool        hi  = false;
      std::string txt;
      if(idx==-2) {
        keysItemRect = r;
        hi  = keysItemSel;
        txt = ArpgMenu::gtxt("Sterowanie");
        } else {
        auto& it = hItems[idx];
        it.modernRect = r;
        hi = (&it==sel) && !keysItemSel;
        getText(it, textBuf);
        txt = textBuf.data();
        }
      if(hi)
        band(r);
      auto& f = hi ? fSel : fItem;
      f.drawText(p, x0 + (hi ? int(10.f*k) : 0), y + itemH/2 + f.pixelSize()/3, txt);
      y += itemH;
      }

    // what the selected item does (the second text of the script), bottom left
    if(keysItemSel) {
      fInfo.drawText(p, x0, H-int(36.f*k), ArpgMenu::gtxt("Klawisze trybu ARPG: zobacz i zmień"));
      }
    else if(sel!=nullptr && sel->handle!=nullptr && sel->handle->text->size()>0) {
      std::string_view info = sel->handle->text[1];
      if(!info.empty())
        fInfo.drawText(p, x0, H-int(36.f*k), info);
      }
    }

  // the version of the game, bottom right
  if(owner.hasVersionLine()) {
    const auto ts = fInfo.textSize(appBuild);
    fInfo.drawText(p, W-ts.w-int(28.f*k), H-int(28.f*k), appBuild);
    }
  }

// "Sterowanie": the keys of the ARPG mode (click / Enter = press a new key), the fixed ones below
void GameMenu::paintModernKeys(Painter& p, GthFont& fItem, GthFont& fSel, GthFont& fInfo,
                               const std::function<void(const Rect&)>& band) {
  const int   W = w(), H = h();
  const float k = float(H)/1080.f;
  const float base = float(std::max(1, Resources::font(1.f).pixelSize()));
  GthFont fRow  = Resources::font(Resources::FontType::Normal, 26.f*k/base);
  GthFont fRowH = Resources::font(Resources::FontType::Hi,     26.f*k/base);
  GthFont fKey  = Resources::font(Resources::FontType::Hi,     26.f*k/base);
  fRow .setColor(Color(0.90f, 0.85f, 0.76f, 1.f));
  fRowH.setColor(Color(1.00f, 0.82f, 0.45f, 1.f));
  fKey .setColor(Color(1.00f, 0.90f, 0.62f, 1.f));

  const int x0 = int(float(W)*0.06f);
  int       y  = int(float(H)*0.06f);
  fSel.drawText(p, x0, y+fSel.pixelSize(), ArpgMenu::gtxt("Sterowanie"));
  y += fSel.pixelSize() + int(10.f*k);
  fInfo.drawText(p, x0, y+fInfo.pixelSize(), ArpgMenu::gtxt("Kliknij albo Enter, potem naciśnij nowy klawisz (Esc - bez zmiany)"));
  y += fInfo.pixelSize() + int(22.f*k);

  const int rowH  = int(38.f*k);
  const int rowW  = int(float(W)*0.46f);
  const int keyX  = x0 + int(float(rowW)*0.62f);
  const int count = int(ArpgKeys::Count);
  for(int i=0; i<count+2; ++i) {
    const Rect r(x0-int(14.f*k), y, rowW, rowH);
    keysRows.push_back(r);
    const bool hi = (keysSel==i);
    if(hi)
      band(r);
    auto& f = hi ? fRowH : fRow;
    const int ty = y + rowH/2 + f.pixelSize()/3;
    if(i<count) {
      auto a = ArpgKeys::Action(i);
      f.drawText(p, x0 + (hi ? int(8.f*k) : 0), ty, ArpgMenu::gtxt(ArpgKeys::info(a).name));
      const std::string key = (hi && keysCapture) ? std::string("Naciśnij klawisz...") : ArpgKeys::keyName(ArpgKeys::key(a));
      fKey.drawText(p, keyX, ty, ArpgMenu::gtxt(key));
      } else {
      if(i==count)
        y += int(10.f*k);
      const char* txt = (i==count) ? "Przywróć domyślne" : "Wróć";
      keysRows.back().y = y;
      f.drawText(p, x0 + (hi ? int(8.f*k) : 0), y + rowH/2 + f.pixelSize()/3, ArpgMenu::gtxt(txt));
      }
    y += rowH;
    }

  // the fixed ones
  y += int(18.f*k);
  static const char* fixed[] = {
    "Lewy przycisk myszy - ruch, rozmowa, podnoszenie, atak",
    "Prawy przycisk - atak ładowany (przytrzymaj)",
    "Środkowy przycisk - blok, z łukiem odepchnięcie, bez broni obrót kamery",
    "Ctrl + lewy przycisk - atak w miejscu / na każdego",
    "Kółko myszy - przybliżenie kamery",
    "3 - 0 - runy i zwoje (przypisanie w ekwipunku)",
    "Ruch klawiaturą, obrót kamery, skok - z ustawień Gothica (Opcje)",
    };
  for(auto t:fixed) {
    fInfo.drawText(p, x0, y+fInfo.pixelSize(), ArpgMenu::gtxt(t));
    y += fInfo.pixelSize() + int(8.f*k);
    }
  (void)fItem;
  }

void GameMenu::modernActivate() {
  const int count = int(ArpgKeys::Count);
  if(keysSel<count) {
    keysCapture = true;
    }
  else if(keysSel==count) {
    ArpgKeys::resetDefaults();
    Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_SELECT"));
    }
  else {
    modernPage  = 0;
    keysCapture = false;
    }
  update();
  }

bool GameMenu::modernKeyDown(KeyEvent& e) {
  if(!modern || modernPage!=1)
    return false;
  if(keysCapture) {
    if(e.key!=Event::K_ESCAPE && keysSel<int(ArpgKeys::Count)) {
      if(!ArpgKeys::canBind(e.key))
        return true; // 3-0 are the spell slots: wait for another key
      ArpgKeys::setKey(ArpgKeys::Action(keysSel), e.key);
      Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_SELECT"));
      }
    keysCapture = false;
    update();
    return true;
    }
  if(e.key==Event::K_ESCAPE || e.key==Event::K_Back) {
    modernPage = 0;
    update();
    return true;
    }
  return false;
  }

bool GameMenu::modernBack() {
  if(!modern || modernPage!=1)
    return false;
  if(keysCapture)
    keysCapture = false; else
    modernPage = 0;
  update();
  return true;
  }

bool GameMenu::modernMouse(const Point& pos, bool click) {
  if(!modern)
    return false;
  auto inside = [&](const Rect& r) { return r.w>0 && pos.x>=r.x && pos.y>=r.y && pos.x<r.x+r.w && pos.y<r.y+r.h; };
  if(modernPage==1) {
    if(keysCapture)
      return true;
    for(size_t i=0; i<keysRows.size(); ++i) {
      if(!inside(keysRows[i]))
        continue;
      if(keysSel!=int(i)) {
        keysSel = int(i);
        Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_BROWSE"));
        update();
        }
      if(click)
        modernActivate();
      return true;
      }
    return false;
    }
  if(inside(keysItemRect)) {
    if(!keysItemSel) {
      keysItemSel = true;
      curItem     = uint32_t(-1);
      Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_BROWSE"));
      update();
      }
    if(click)
      onKeyboard(KeyCodec::ActionGeneric);
    return true;
    }
  for(int i=0; i<zenkit::IMenu::item_count; ++i) {
    if(!inside(hItems[i].modernRect))
      continue;
    if(curItem!=uint32_t(i) || keysItemSel) {
      keysItemSel = false;
      Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_BROWSE"));
      setSelection(i, 1);
      if(auto s = selectedItem())
        updateSavThumb(*s);
      update();
      }
    if(click)
      onKeyboard(KeyCodec::ActionGeneric);
    return true;
    }
  return false;
  }

void GameMenu::drawItem(Painter& p, Item& hItem) {
  if(!hItem.visible || hItem.name.empty())
    return;
  if(isHidden(hItem.handle))
    return;

  auto& item  = hItem.handle;
  auto flags = item->flags;
  getText(hItem,textBuf);

  const int32_t dimx = (item->dim_x!=-1) ? item->dim_x : 8192;
  const int32_t dimy = (item->dim_y!=-1) ? item->dim_y : 750;

  const int   x     = int(float(w()*item->pos_x)/scriptDiv);
  const int   y     = int(float(h()*item->pos_y)/scriptDiv);
  int         szX   = int(float(w()*dimx       )/scriptDiv);
  int         szY   = int(float(h()*dimy       )/scriptDiv);

  if(hItem.img && !hItem.img->isEmpty() && Resources::isDiathicLogo(item->backpic)) {
    // Diathic: the logo keeps its proportions, 1.5x the height of Gothic's headline, flames over the top of the menu,
    // the glowing line at the bottom just above the first item
    const float aspect = float(hItem.img->w())/float(std::max(1,hItem.img->h()));
    int lh = int(float(szY)*1.5f);
    int lw = int(float(lh)*aspect);
    if(lw>szX) {
      lw = szX;
      lh = int(float(lw)/aspect);
      }
    p.setBrush(*hItem.img);
    p.drawRect(x+(szX-lw)/2, y+szY-lh, lw, lh,
               0,0,hItem.img->w(),hItem.img->h());
    }
  else if(hItem.img && !hItem.img->isEmpty()) {
    p.setBrush(*hItem.img);
    p.drawRect(x,y,szX,szY,
               0,0,hItem.img->w(),hItem.img->h());
    }

  auto fnt = getTextFont(hItem);
  int  tw  = szX;
  int  th  = szY;

  AlignFlag txtAlign=NoAlign;
  if(flags & zenkit::MenuItemFlag::CENTERED) {
    txtAlign = AlignHCenter | AlignVCenter;
    }

  //p.setBrush(Color(1,1,1,1));
  //p.drawRect(x,y,szX,szY);
  {
  int padd = 0;
  if((flags & zenkit::MenuItemFlag::MULTILINE) &&
     std::min(tw,th)>100*2+fnt.pixelSize()) {
    padd = 100; // TODO: find out exact padding formula
    }
  auto tRect   = Rect(x+padd,y+fnt.pixelSize()+padd,
                      tw-2*padd, th-2*padd);

  if(flags & zenkit::MenuItemFlag::MULTILINE) {
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

  if(item->type==zenkit::MenuItemType::SLIDER && slider!=nullptr) {
    drawSlider(p,hItem,x,y,szX,szY);
    }
  else if(item->type==zenkit::MenuItemType::LISTBOX) {
    if(auto ql = Gothic::inst().questLog()) {
      const int px = int(float(w()*item->frame_sizex)/scriptDiv);
      const int py = int(float(h()*item->frame_sizey)/scriptDiv);

      auto st = toStatus(item->user_string[0]);
      drawQuestList(p, hItem, x+px,y+py, szX-2*px,szY-2*py, *ql,st);
      }
    }
  else if(item->type==zenkit::MenuItemType::INPUT) {
    string_frm textBuf;
    if(item->on_chg_set_option_section=="KEYS") {
      auto keys = Gothic::settingsGetS(item->on_chg_set_option_section, item->on_chg_set_option);
      if(&hItem!=ctrlInput)
        textBuf = KeyCodec::keysStr(keys); else
        textBuf = "_";
      }
    else {
      auto str = string_frm(item->text[0]);
      if(str.empty() && &hItem!=ctrlInput && saveSlotId(hItem)!=size_t(-1))
        str = "---";
      if(&hItem!=ctrlInput)
        textBuf = std::string_view(str); else
        textBuf = string_frm(str,"_");
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

  auto& sec = it.handle->on_chg_set_option_section;
  auto& opt = it.handle->on_chg_set_option;
  if(sec.empty() || opt.empty())
    return;

  float v  = Gothic::settingsGetF(sec, opt);
  int   dx = int(float(sw-w)*std::max(0.f,std::min(v,1.f)));
  p.drawRect(x+dx,y+(sh-h)/2,w,h,
             0,0,p.brush().w(),p.brush().h());
  }

void GameMenu::drawQuestList(Painter& p, Item& it, int x, int y, int w, int h,
                             const QuestLog& log, QuestStat st) {
  int     itY       = y;
  int32_t listId    = -1;
  int32_t listLen   = numQuests(&log,st);
  int32_t listBegin = it.scroll;
  int32_t listEnd   = listLen;

  const float scale = Gothic::interfaceScale(this);
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

    auto& fnt = Resources::font(it.handle->fontname,ft,scale);
    auto sz  = fnt.textSize(w,quest.name);
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

void GameMenu::onKeyboard(KeyCodec::Action key) {
  if(modern && modernPage==1) {
    // Diathic: "Sterowanie"
    const int rows = int(ArpgKeys::Count)+2;
    if(key==KeyCodec::Forward || key==KeyCodec::Back) {
      Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_BROWSE"));
      keysSel = (keysSel + (key==KeyCodec::Forward ? rows-1 : 1)) % rows;
      update();
      }
    if(key==KeyCodec::ActionGeneric)
      modernActivate();
    return;
    }
  if(modern && (key==KeyCodec::Forward || key==KeyCodec::Back)) {
    // Diathic: up / down in the order of the modern list, "Sterowanie" included
    auto order = modernOrder();
    if(!order.empty()) {
      int at = -1;
      for(size_t i=0; i<order.size(); ++i)
        if((order[i]==-2 && keysItemSel) || (order[i]>=0 && !keysItemSel && uint32_t(order[i])==curItem))
          at = int(i);
      const int n  = int(order.size());
      const int nx = at<0 ? 0 : (at + (key==KeyCodec::Forward ? n-1 : 1)) % n;
      Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_BROWSE"));
      if(order[size_t(nx)]==-2) {
        keysItemSel = true;
        curItem     = uint32_t(-1);
        } else {
        keysItemSel = false;
        setSelection(order[size_t(nx)], 1);
        if(auto s = selectedItem())
          updateSavThumb(*s);
        }
      update();
      }
    return;
    }
  if(modern && keysItemSel && key==KeyCodec::ActionGeneric) {
    Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_SELECT"));
    modernPage  = 1;
    keysSel     = 0;
    keysCapture = false;
    update();
    return;
    }
  auto sel = selectedItem();
  if(sel!=nullptr && isHorSelectable(sel->handle)) {
    if(key==KeyCodec::Left)
      key = KeyCodec::Forward;
    else if(key==KeyCodec::Right)
      key = KeyCodec::Back;
    }

  if(key==KeyCodec::Forward || key==KeyCodec::Back) {
    Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_BROWSE"));
    const int dy = (key==KeyCodec::Forward) ? -1 : +1;
    setSelection(int(curItem)+dy, dy);
    if(auto s = selectedItem())
      updateSavThumb(*s);
    update();
    }

  if((key==KeyCodec::Left || key==KeyCodec::Right) && sel!=nullptr) {
    const int dx = (key==KeyCodec::Left) ? -1 : +1;
    exec(*sel,dx,key);
    }

  if(key==KeyCodec::ActionGeneric && sel!=nullptr) {
    Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_SELECT"));
    exec(*sel,0,key);
    }
  if(key==KeyCodec::K_Del && sel!=nullptr) {
    Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_SELECT"));
    exec(*sel,0,key);
    }
  }

void GameMenu::onTick() {
  update();

  const float scale = Gothic::interfaceScale(this);
  const float fx    = 640.0f * scale;
  const float fy    = 480.0f * scale;

  const float wx = float(owner.w());
  const float wy = float(owner.h());

  Size size = {0, 0};
  if(modern) {
    size = {owner.w(), owner.h()}; // Diathic: the modern main menu covers the screen
    }
  else if(menu->flags & zenkit::MenuFlag::DONT_SCALE_DIMENSION) {
    size = {int(float(menu->dim_x)*fx/scriptDiv),int(float(menu->dim_y)*fy/scriptDiv)};
    } else {
    size = {int(float(menu->dim_x)*wx/scriptDiv),int(float(menu->dim_y)*wy/scriptDiv)};
    }
  resize(size);

  if(menu->flags & zenkit::MenuFlag::ALIGN_CENTER) {
    setPosition((owner.w()-w())/2, (owner.h()-h())/2);
    } else {
    setPosition(int(float(menu->pos_x)/scriptDiv*fx), int(float(menu->pos_y)/scriptDiv*fy));
    }
  }

void GameMenu::processMusicTheme() {
  // Diathic: our track in the main menu before the game (in the game the menu keeps Gothic's music)
  if(!Gothic::inst().isInGame() && menu->music_theme.size()==8 &&
     std::equal(menu->music_theme.begin(), menu->music_theme.end(), "SYS_MENU",
                [](char a, char b){ return std::toupper(uint8_t(a))==b; })) {
    GameMusic::inst().playMenuTrack();
    return;
    }
  if(auto theme = Gothic::musicDef()[menu->music_theme])
    GameMusic::inst().setMusic(*theme,GameMusic::mkTags(GameMusic::Std,GameMusic::Day));
  }

GameMenu::Item *GameMenu::selectedItem() {
  if(curItem<zenkit::IMenu::item_count)
    return &hItems[curItem];
  return nullptr;
  }

GameMenu::Item* GameMenu::selectedNextItem(Item *it) {
  uint32_t cur=curItem+1;
  for(uint32_t i=0;i<zenkit::IMenu::item_count;++i)
    if(&hItems[i]==it) {
      cur=i+1;
      break;
      }

  for(int i=0;i<zenkit::IMenu::item_count;++i,cur++) {
    cur%=zenkit::IMenu::item_count;

    auto& it=hItems[cur].handle;
    if(isEnabled(it))
      return &hItems[cur];
    }
  return nullptr;
  }

GameMenu::Item* GameMenu::selectedContentItem(Item *it) {
  uint32_t cur=curItem+1;
  for(uint32_t i=0;i<zenkit::IMenu::item_count;++i)
    if(&hItems[i]==it) {
      cur=i+1;
      break;
      }

  for(int i=0;i<zenkit::IMenu::item_count;++i,cur++) {
    cur%=zenkit::IMenu::item_count;

    auto& it=hItems[cur].handle;
    if(isEnabled(it) && it->type==zenkit::MenuItemType::TEXT)
      return &hItems[cur];
    }
  return nullptr;
  }

void GameMenu::setSelection(int desired, int seek) {
  uint32_t cur=uint32_t(desired);
  for(int i=0; i<zenkit::IMenu::item_count; ++i,cur+=uint32_t(seek)) {
    cur%=zenkit::IMenu::item_count;

    auto& it=hItems[cur].handle;
    if(isSelectable(it) && isEnabled(it)){
      curItem=cur;
      for(size_t i=0;i<zenkit::IMenuItem::select_action_count;++i)
        if(it->on_sel_action[i]==int(zenkit::MenuItemSelectAction::EXECUTE_COMMANDS))
          execCommands(it->on_sel_action_s[i],false,KeyCodec::Idle);
      return;
      }
    }
  curItem=uint32_t(-1);
  }

void GameMenu::getText(const Item& it, std::vector<char> &out) {
  if(out.size()==0)
    out.resize(1);
  out[0]='\0';

  const auto& src = it.handle->text[0];
  if(it.handle->type==zenkit::MenuItemType::TEXT) {
    size_t size = src.size();
    out.resize(size+1);
    std::memcpy(out.data(),src.c_str(),size+1);
    return;
    }

  if(it.handle->type==zenkit::MenuItemType::CHOICEBOX) {
    strEnum(src,it.value,out);
    return;
    }
  }

const GthFont& GameMenu::getTextFont(const GameMenu::Item &it) {
  const float scale = Gothic::interfaceScale(this);
  GthFont ret;
  if(!isEnabled(it.handle))
    return Resources::font(it.handle->fontname, Resources::FontType::Disabled, scale);
  if(&it==selectedItem())
    return Resources::font(it.handle->fontname, Resources::FontType::Hi, scale);
  return Resources::font(it.handle->fontname, Resources::FontType::Normal, scale);
  }

bool GameMenu::isSelectable(const std::shared_ptr<zenkit::IMenuItem>& item) {
  return item != nullptr && (item->flags & zenkit::MenuItemFlag::SELECTABLE) && !isHidden(item);
  }

bool GameMenu::isHorSelectable(const std::shared_ptr<zenkit::IMenuItem>& item) {
  return item != nullptr && (item->flags & zenkit::MenuItemFlag::HOR_SELECTABLE) && !isHidden(item);
  }

bool GameMenu::isEnabled(const std::shared_ptr<zenkit::IMenuItem>& item) {
  if(item==nullptr)
    return false;
  if((item->flags & zenkit::MenuItemFlag::ONLY_INGAME) && !Gothic::inst().isInGameAndAlive())
    return false;
  if((item->flags & zenkit::MenuItemFlag::ONLY_OUTGAME) && Gothic::inst().isInGameAndAlive())
    return false;
  return true;
  }

bool GameMenu::isHidden(const std::shared_ptr<zenkit::IMenuItem>& item) {
  if(item==nullptr)
    return false;
  if(item->hide_if_option_set.empty())
    return false;

  auto opt = Gothic::inst().settingsGetI(item->hide_if_option_section_set,
                                         item->hide_if_option_set);
  return opt==item->hide_on_value;
  }

void GameMenu::exec(Item &p, int slideDx, KeyCodec::Action hint) {
  auto* it = &p;
  while(it!=nullptr) {
    if(it==&p)
      execSingle(*it,slideDx,hint); else
      execChgOption(*it,slideDx);
    if(it->handle->flags & zenkit::MenuItemFlag::EFFECTS) {
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

void GameMenu::execSingle(Item &it, int slideDx, KeyCodec::Action hint) {
  auto& item          = it.handle;
  auto& onSelAction   = item->on_sel_action;
  auto& onSelAction_S = item->on_sel_action_s;
  auto& onEventAction = item->on_event_action;

  if(item->type==zenkit::MenuItemType::INPUT && slideDx==0) {
    ctrlInput = &it;
    if(item->on_chg_set_option.empty()) {
      SavNameDialog dlg{item->text[0]};
      if(it.savHdr.version==0)
        dlg.text = "";
      dlg.resize(owner.size());
      dlg.exec();
      ctrlInput = nullptr;
      if(!dlg.accepted)
        return;
      }
    else if(hint==KeyCodec::K_Del) {
      keyCodec.clear(item->on_chg_set_option_section, item->on_chg_set_option);
      updateItem(it);
      ctrlInput = nullptr;
      return; //HACK: there is a SEL_ACTION_BACK token in same item
      }
    else {
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
      keyCodec.set(item->on_chg_set_option_section, item->on_chg_set_option, next);
      updateItem(it);
      return; //HACK: there is a SEL_ACTION_BACK token in same item
      }
    }

  for(size_t i=0; i<zenkit::IMenuItem::select_action_count; ++i) {
    auto action = zenkit::MenuItemSelectAction(onSelAction[i]);
    switch(action) {
      case zenkit::MenuItemSelectAction::UNKNOWN:
        break;
      case zenkit::MenuItemSelectAction::BACK:
        Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_ESC"));
        exitFlag = true;
        break;
      case zenkit::MenuItemSelectAction::START_MENU:
        if(vm->find_symbol_by_name(onSelAction_S[i]) != nullptr)
          owner.pushMenu(new GameMenu(owner,keyCodec,*vm,onSelAction_S[i],keyClose()));
        break;
      case zenkit::MenuItemSelectAction::START_ITEM:
        break;
      case zenkit::MenuItemSelectAction::CLOSE:
        Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_ESC"));

        if(onSelAction_S[i]=="NEW_GAME") {
          Gothic::inst().onStartGame(Gothic::inst().defaultWorld());
          }
        else if(onSelAction_S[i]=="LEAVE_GAME") {
          Log::i("Exiting, by item action (`LEAVE_GAME`)");
          Tempest::SystemApi::exit();
          }
        else if(onSelAction_S[i]=="SAVEGAME_SAVE") {
          execSaveGame(it);
          }
        else if(onSelAction_S[i]=="SAVEGAME_LOAD"){
          if (!execLoadGame(it)){
            break;
          }
        }
        closeFlag = true;
        break;
      case zenkit::MenuItemSelectAction::CON_COMMANDS:
        // TODO: inline marvin commands
        break;
      case zenkit::MenuItemSelectAction::PLAY_SOUND:
        Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx(onSelAction_S[i]));
        break;
      case zenkit::MenuItemSelectAction::EXECUTE_COMMANDS:
        execCommands(onSelAction_S[i],true,hint);
        break;
      }
    }

  if(onEventAction[int(zenkit::MenuItemSelectEvent::EXECUTE)]>0){
    auto* sym = vm->find_symbol_by_index(uint32_t(onEventAction[int(zenkit::MenuItemSelectEvent::EXECUTE)]));
    if (sym != nullptr)
      vm->call_function(sym);
    }

  execChgOption(it,slideDx);
  }

void GameMenu::execChgOption(Item &item, int slideDx) {
  auto& sec = item.handle->on_chg_set_option_section;
  auto& opt = item.handle->on_chg_set_option;
  if(sec.empty() || opt.empty())
    return;

  if(item.handle->type==zenkit::MenuItemType::SLIDER && slideDx!=0) {
    updateItem(item);
    float v = Gothic::settingsGetF(sec, opt);
    v  = std::max(0.f,std::min(v+float(slideDx)*0.03f,1.f));
    Gothic::settingsSetF(sec, opt, v);
    }
  if(item.handle->type==zenkit::MenuItemType::CHOICEBOX) {
    updateItem(item);
    const int cnt = int(strEnumSize(item.handle->text[0]));
    if(slideDx==0 && cnt==2)
      slideDx = 1; // QoL: on/off toggle

    item.value += slideDx; // next value
    if(cnt>0)
      item.value = std::clamp(item.value,0,cnt-1);  else
      item.value = 0;
    Gothic::settingsSetI(sec, opt, item.value);
    }
  }

void GameMenu::execSaveGame(const GameMenu::Item& item) {
  const size_t id = saveSlotId(item);
  if(id==size_t(-1))
    return;

  string_frm fname("save_slot_",int(id),".sav");
  Gothic::inst().save(fname,item.handle->text[0]);
  }

bool GameMenu::execLoadGame(const GameMenu::Item &item) {
  const size_t id = saveSlotId(item);
  if(id==size_t(-1))
    return false;

  string_frm fname("save_slot_",int(id),".sav");
  if(!FileUtil::exists(TextCodec::toUtf16(fname.c_str())))
    return false;
  Gothic::inst().load(fname);
  return true;
  }

void GameMenu::execCommands(std::string str, bool isClick, KeyCodec::Action hint) {
  if(str.find("EFFECTS ")==0) {
    // menu log
    const char* arg0 = str.data()+std::strlen("EFFECTS ");
    for(uint32_t id=0; id<zenkit::IMenu::item_count; ++id) {
      auto& i = hItems[id];
      if(i.handle != nullptr && i.handle->type==zenkit::MenuItemType::LISTBOX) {
        i.visible = (i.name==arg0);
        if(i.visible && isClick) {
          const uint32_t prev = curItem;
          curItem = id;
          ListViewDialog dlg(*this,i);
          if(dlg.numQuests()==0) {
            curItem = prev;
            return;
            }
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
    const char* what = str.data()+4;
    while(*what==' ')
      ++what;
    for(auto& i:hItems)
      if(i.name==what)
        execSingle(i,0,hint);
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
  item.value = Gothic::settingsGetI(it->on_chg_set_option_section, it->on_chg_set_option);
  updateSavTitle(item);
  }

void GameMenu::updateSavTitle(GameMenu::Item& sel) {
  if(sel.handle->on_sel_action_s[0]!="SAVEGAME_LOAD" &&
     sel.handle->on_sel_action_s[1]!="SAVEGAME_SAVE")
    return;
  const size_t id = saveSlotId(sel);
  if(id==size_t(-1))
    return;

  char fname[64]={};
  std::snprintf(fname,sizeof(fname)-1,"save_slot_%d.sav",int(id));

  if(!FileUtil::exists(TextCodec::toUtf16(fname))) {
    sel.handle->text[0] = "---";
    return;
    }

  SaveGameHeader hdr;
  try {
    RFile     fin(fname);
    Serialize reader(fin);
    reader.setEntry("header");
    reader.read(hdr);
    if(id!=0 || sel.handle->text[0].empty())
      sel.handle->text[0] = hdr.name;
    sel.savHdr = std::move(hdr);

    if(reader.setEntry("priview.png"))
      reader.read(sel.savPriview); // legacy
    else if(reader.setEntry("preview.png"))
      reader.read(sel.savPriview);
    else if(reader.setEntry("preview.jpg"))
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
  if(sel.handle->on_sel_action_s[0]!="SAVEGAME_LOAD" &&
     sel.handle->on_sel_action_s[1]!="SAVEGAME_SAVE")
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
  set("MENUITEM_VID_RESOLUTION_CHOICE", "full|upscale(75%)|upscale(half)");
  }

void GameMenu::setDefaultKeys(std::string_view preset) {
  keyCodec.setDefaultKeys(preset);
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
  set("MENUITEM_LOADSAVE_LEVELNAME_VALUE",hdr.world);

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

std::string_view GameMenu::strEnum(std::string_view en, int id, std::vector<char> &out) {
  int num=0;

  size_t i=0;
  for(size_t r=0; r<en.size(); ++r)
    if(en[r]=='#'){
      i=r+1;
      break;
      }

  for(;i<en.size();++i){
    size_t b=i;
    for(size_t r=i; r<en.size(); ++r,++i)
      if(en[r]=='|')
        break;

    if(id==num) {
      size_t sz=i-b;
      out.resize(sz+1);
      std::memcpy(&out[0],&en[b],sz);
      out[sz]='\0';
      return out.data();
      }
    ++num;
    }

  for(size_t i=0; i<en.size(); ++i){
    if(en[i]=='#' || en[i]=='|'){
      out.resize(i+1);
      std::memcpy(&out[0],en.data(),i);
      out[i]='\0';
      return out.data();
      }
    }

  return "";
  }

size_t GameMenu::strEnumSize(std::string_view en) {
  if(en.empty())
    return 0;
  size_t cnt = 0;
  for(size_t i=0; i<en.size(); ++i) {
    if(en[i]=='#' || en[i]=='|') {
      cnt += en[i]=='|' ? 2 : 1;
      i++;
      for(;i<en.size();++i) {
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
  set(item,string_frm(value));
  }

void GameMenu::set(std::string_view item, const int32_t value) {
  set(item,string_frm(value));
  }

void GameMenu::set(std::string_view item, const int32_t value, const int32_t max) {
  set(item,string_frm(value,"/",max));
  }

void GameMenu::set(std::string_view item, std::string_view value) {
  for(auto& i:hItems)
    if(i.name==item) {
      i.handle->text[0] = value;
      return;
      }
  }

void GameMenu::updateValues() {
  gtime time;
  if(auto w = Gothic::inst().world())
    time = w->time();

  set("MENU_ITEM_PLAYERGUILD","Debugger");
  set("MENU_ITEM_LEVEL","0");
  set("MENUITEM_AUDIO_PROVIDER_CHOICE", "OpenGothic|GothicKit (Experimental)");
  for(auto& i:hItems) {
    if(i.name=="MENU_ITEM_CONTENT_VIEWER") {
      i.visible=false;
      }
    if(i.name=="MENU_ITEM_DAY") {
      i.handle->text[0] = std::to_string(time.day());
      }
    if(i.name=="MENU_ITEM_TIME") {
      char form[64]={};
      std::snprintf(form,sizeof(form),"%d:%02d",int(time.hour()),int(time.minute()));
      i.handle->text[0] = form;
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
  auto* gilds = sc.findSymbol("TXT_GUILDS");
  auto* tal   = sc.findSymbol("TXT_TALENTS");
  auto* talV  = sc.findSymbol("TXT_TALENTS_SKILLS");

  if(gilds==nullptr || tal==nullptr || talV==nullptr) {
    return;
    }

  set("MENU_ITEM_PLAYERGUILD", gilds->get_string(uint16_t(pl.guild())));

  set("MENU_ITEM_LEVEL",       pl.level());
  set("MENU_ITEM_EXP",         pl.experience());
  set("MENU_ITEM_LEVEL_NEXT",  pl.experienceNext());
  set("MENU_ITEM_LEARN",       pl.learningPoints());

  set("MENU_ITEM_ATTRIBUTE_1", pl.attribute(ATR_STRENGTH));
  set("MENU_ITEM_ATTRIBUTE_2", pl.attribute(ATR_DEXTERITY));
  set("MENU_ITEM_ATTRIBUTE_3", pl.attribute(ATR_MANA),      pl.attribute(ATR_MANAMAX));
  set("MENU_ITEM_ATTRIBUTE_4", pl.attribute(ATR_HITPOINTS), pl.attribute(ATR_HITPOINTSMAX));

  set("MENU_ITEM_ARMOR_11",    pl.protection(PROT_EDGE));
  set("MENU_ITEM_ARMOR_1",     pl.protection(PROT_BLUNT));
  set("MENU_ITEM_ARMOR_2",     pl.protection(PROT_POINT)); // not sure about it
  set("MENU_ITEM_ARMOR_3",     pl.protection(PROT_FIRE));
  set("MENU_ITEM_ARMOR_4",     pl.protection(PROT_MAGIC));

  const bool g2        = Gothic::inst().version().game==2;
  const int  talentMax = g2 ? TALENT_MAX_G2 : TALENT_MAX_G1;
  for(uint16_t i=0; i<talentMax; ++i) {
    auto& str = tal->get_string(i);
    if(str.empty())
      continue;

    const int sk  = pl.talentSkill(Talent(i));
    const int val = g2 ? pl.hitChance(Talent(i)) : pl.talentValue(Talent(i));

    set(string_frm("MENU_ITEM_TALENT_",i,"_TITLE"), str);
    set(string_frm("MENU_ITEM_TALENT_",i,"_SKILL"), strEnum(talV->get_string(i),sk,textBuf));
    set(string_frm("MENU_ITEM_TALENT_",i),          string_frm(val,"%"));
    }
  }
