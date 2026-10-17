#include "gamemenu.h"
#include "ui/cursorshape.h"

#include <Tempest/Painter>
#include <Tempest/Log>
#include <Tempest/TextCodec>
#include <Tempest/Dialog>

#include <algorithm>
#include <cctype>

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
#include "build.h"

using namespace Tempest;

static const float scriptDiv=8192.0f;

struct GameMenu::ListContentDialog : Dialog {
  ListContentDialog(Item& textView):textView(textView) {
    setFocusPolicy(ClickFocus);
    setCursorShape(HiddenCursor);
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
    setCursorShape(HiddenCursor);
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
    setCursorShape(HiddenCursor);
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
    setCursorShape(HiddenCursor);
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
  setCursorShape(HiddenCursor);
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
#ifdef OPENGOTHIC_TOUCH_UI
  {
    // character sheet and log keep their original layout
    std::string section(menuSection);
    for(auto& ch:section)
      ch = char(std::toupper(static_cast<unsigned char>(ch)));
    touch.log    = section.find("MENU_LOG")!=std::string::npos && section.find("STATUS")==std::string::npos;
    touch.list = section.find("STATUS")==std::string::npos && section.find("LOG")==std::string::npos;
  }
#endif

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

#ifdef OPENGOTHIC_TOUCH_UI
namespace {
// Layout of the touch list, in fractions of the menu size
constexpr float TouchLeft       = 0.035f;
constexpr float TouchRight      = 0.965f;
constexpr float TouchTop        = 0.20f;
constexpr float TouchBottom     = 0.902f;
constexpr float TouchRowHeight  = 0.078f;
constexpr int   TouchRows       = 9;
constexpr float TouchLabelX     = 0.10f;
constexpr float TouchControlX   = 0.49f; // sliders react to touches on the right of it
constexpr float TouchSliderX    = 0.52f;
constexpr float TouchSliderW    = 0.30f;
constexpr float TouchChoiceMid  = 0.66f; // a tap on the left of it selects the previous value
}

// Label and value of a row are separate items, linked through the EFFECTS flag
uint32_t GameMenu::touchControl(uint32_t row) const {
  uint32_t at = row;
  for(unsigned n=0; n<zenkit::IMenu::item_count; ++n) {
    const auto& h = hItems[at].handle;
    if(!h)
      return row;
    if(h->type==zenkit::MenuItemType::SLIDER || h->type==zenkit::MenuItemType::CHOICEBOX ||
       h->type==zenkit::MenuItemType::INPUT)
      return at;
    if(!(h->flags & zenkit::MenuItemFlag::EFFECTS))
      return row;
    do {
      at = (at+1)%zenkit::IMenu::item_count;
      } while(at!=row && !isEnabled(hItems[at].handle));
    if(at==row)
      return row;
    }
  return row;
  }

std::vector<uint32_t> GameMenu::touchRows() const {
  std::vector<uint32_t> rows;
  for(uint32_t i=0; i<zenkit::IMenu::item_count; ++i) {
    const auto& item = hItems[i];
    if(!item.visible || (!isSelectable(item.handle) && !isHorSelectable(item.handle)))
      continue;

    bool paired = false;
    for(auto prev:rows) {
      if(touchControl(prev)==i)
        paired = true;
      }
    if(!paired)
      rows.push_back(i);
    }
  return rows;
  }

int GameMenu::touchRowAt(float px, float py) const {
  if(px<float(w())*TouchLeft || px>float(w())*TouchRight ||
     py<float(h())*TouchTop  || py>=float(h())*TouchBottom)
    return -1;

  const auto rows = touchRows();
  const int  n    = touch.offset+int((py/float(h())-TouchTop)/TouchRowHeight);
  return n>=0 && size_t(n)<rows.size() ? int(rows[size_t(n)]) : -1;
  }

/// phase: 0 - down, 1 - move, 2 - up, 3 - cancel
void GameMenu::mobileTouch(float px, float py, int phase) {
  if(touch.log) {
    mobileLogTouch(px,py,phase);
    return;
    }
  if(phase==3 || !isTouchMenu() || ctrlInput!=nullptr) {
    touch.slider   = -1;
    touch.dragging = false;
    touch.consumed = false;
    return;
    }

  if(phase==0) {
    touch.slider   = -1;
    touch.consumed = false;
    touch.dragging = false;
    touch.startX   = px;
    touch.startY   = py;

    const int row = touchRowAt(px,py);
    if(row<0 || !isEnabled(hItems[row].handle) || px<float(w())*TouchControlX)
      return;
    const auto id = touchControl(uint32_t(row));
    auto&      h  = hItems[id].handle;
    if(h->type!=zenkit::MenuItemType::SLIDER || h->on_chg_set_option.empty())
      return;
    touch.slider  = int(id);
    touch.preview = Gothic::settingsGetF(h->on_chg_set_option_section,h->on_chg_set_option);
    setSelection(row);
    return;
    }

  if(touch.slider<0)
    return;

  // vertical swipes scroll the list, they must not change the slider
  const float dx = std::abs(px-touch.startX);
  const float dy = std::abs(py-touch.startY);
  if(!touch.dragging && dy>float(h())*0.035f && dy>dx) {
    touch.slider = -1;
    return;
    }
  if(dx>float(h())*0.02f && dx>dy*1.25f)
    touch.dragging = true;
  if(touch.dragging || phase==2)
    touch.preview = std::clamp((px/float(w())-TouchSliderX)/TouchSliderW,0.f,1.f);
  update();

  if(phase==2) {
    touch.consumed = true;
    auto& item = hItems[touch.slider];
    Gothic::settingsSetF(item.handle->on_chg_set_option_section,item.handle->on_chg_set_option,touch.preview);
    updateItem(item);
    touch.slider   = -1;
    touch.dragging = false;
    }
  }

void GameMenu::mobileScroll(float dy) {
  if(touch.log)
    return;
  if(touch.consumed || ctrlInput!=nullptr)
    return;
  if(!touch.list) {
    onKeyboard(dy<0 ? KeyCodec::Back : KeyCodec::Forward);
    return;
    }
  const int rows = int(touchRows().size());
  touch.offset = std::clamp(touch.offset+(dy<0 ? 2 : -2),0,std::max(0,rows-TouchRows));
  update();
  }

void GameMenu::mobileTap(float px, float py) {
  if(touch.log) {
    mobileLogTap(px,py);
    return;
    }
  if(!isTouchMenu() || ctrlInput!=nullptr || touch.consumed)
    return;

  const int row = touchRowAt(px,py);
  if(row<0 || !isEnabled(hItems[row].handle))
    return;
  setSelection(row);

  const auto id      = touchControl(uint32_t(row));
  auto&      control = hItems[id];
  if(control.handle->type==zenkit::MenuItemType::SLIDER)
    return;
  if(control.handle->type==zenkit::MenuItemType::CHOICEBOX) {
    const int dir = px<float(w())*TouchChoiceMid ? -1 : 1;
    exec(hItems[row],dir,dir<0 ? KeyCodec::Left : KeyCodec::Right);
    } else {
    onKeyboard(KeyCodec::ActionGeneric);
    }
  }

namespace {
const Color Gold     (0.82f,0.66f,0.34f,0.95f);
const Color GoldSoft (0.82f,0.66f,0.34f,0.55f);
const Color Hairline (0.60f,0.64f,0.70f,0.28f);

void touchBar(Painter& p, int x0, int y, int x1, int th, const Color& c) {
  if(x1<=x0)
    return;
  p.setBrush(Brush(c));
  p.drawRect(x0,y-th/2,x1-x0,th);
  }

void touchDiamond(Painter& p, float cx, float cy, float r, const Color& c) {
  p.setBrush(Brush(c));
  p.drawTriangle(cx-r,cy,0.f,0.f, cx,cy-r,0.f,0.f, cx+r,cy,0.f,0.f);
  p.drawTriangle(cx-r,cy,0.f,0.f, cx+r,cy,0.f,0.f, cx,cy+r,0.f,0.f);
  }

/// A line with a diamond in the middle
void touchDivider(Painter& p, int x0, int x1, int y, float ms, const Color& c, float diamond) {
  const int   cx  = (x0+x1)/2;
  const float r   = diamond*ms;
  const int   gap = int(r*2.2f);
  const int   th  = std::max(1,int(ms*1.2f));
  touchBar(p,x0,y,cx-gap,th,c);
  touchBar(p,cx+gap,y,x1,th,c);
  touchDiamond(p,float(cx),float(y),r,c);
  }

void touchStroke(Painter& p, float x0, float y0, float x1, float y1, float thickness, const Color& c) {
  const float dx  = x1-x0;
  const float dy  = y1-y0;
  const float len = std::max(0.001f,std::sqrt(dx*dx+dy*dy));
  const float nx  = -dy/len*thickness/2;
  const float ny  =  dx/len*thickness/2;
  p.setBrush(Brush(c));
  p.drawTriangle(x0+nx,y0+ny,0.f,0.f, x0-nx,y0-ny,0.f,0.f, x1+nx,y1+ny,0.f,0.f);
  p.drawTriangle(x1+nx,y1+ny,0.f,0.f, x0-nx,y0-ny,0.f,0.f, x1-nx,y1-ny,0.f,0.f);
  }

void touchFrame(Painter& p, int x, int y, int w, int h, int th, const Color& border, const Color& body) {
  p.setBrush(Brush(border));
  p.drawRect(x,y,w,h);
  p.setBrush(Brush(body));
  p.drawRect(x+th,y+th,w-2*th,h-2*th);
  }
}

void GameMenu::paintTouch(Painter& p) {
  const auto rows = touchRows();
  touch.offset = std::clamp(touch.offset,0,std::max(0,int(rows.size())-TouchRows));

  const float ms    = std::min(float(w())/1000.f,float(h())/540.f)*Gothic::interfaceScale(this);
  const int   W     = w();
  const int   H     = h();
  const int   left  = int(float(W)*TouchLabelX);
  const int   rowH  = int(float(H)*TouchRowHeight);
  const int   midX  = W/2;

  // the screen is darkened under the text
  p.setBrush(Brush(Color(0.f,0.f,0.f,0.42f)));
  p.drawRect(0,0,W,H);

  // rows with a value (slider, choice) are options, the rest is a list of buttons
  auto isValued = [&](uint32_t index) {
    const auto id = touchControl(index);
    const auto t  = hItems[id].handle->type;
    return t==zenkit::MenuItemType::SLIDER || t==zenkit::MenuItemType::CHOICEBOX ||
           (t==zenkit::MenuItemType::INPUT && hItems[id].handle->on_chg_set_option_section=="KEYS");
    };
  bool options = false;
  for(auto r:rows)
    options = options || isValued(r);

  // title: the heading of the menu, or the game
  std::string heading;
  {
    int best = 1800;
    for(const auto& candidate:hItems) {
      if(!candidate.handle || !candidate.visible || candidate.handle->type!=zenkit::MenuItemType::TEXT ||
         candidate.handle->text[0].empty() || isSelectable(candidate.handle) || candidate.handle->pos_y>=best)
        continue;
      best    = candidate.handle->pos_y;
      heading = candidate.handle->text[0];
      }
    if(heading.empty() || !options)
      heading = Gothic::inst().version().game==2 ? "GOTHIC II" : "GOTHIC";
  }

  auto title = Resources::font(Resources::FontType::Normal,ms*1.45f);
  title.drawText(p,midX-title.textSize(heading).w/2,int(float(H)*0.125f),heading);
  touchDivider(p,int(float(W)*0.28f),int(float(W)*0.72f),int(float(H)*0.155f),ms,Gold,5.f);

  for(int row=0; row<TouchRows && size_t(touch.offset+row)<rows.size(); ++row) {
    const uint32_t index     = rows[size_t(touch.offset+row)];
    const uint32_t controlId = touchControl(index);
    auto&          item      = hItems[index];
    auto&          control   = hItems[controlId];

    const bool selected = &item==selectedItem();
    auto type = !isEnabled(item.handle) ? Resources::FontType::Disabled :
                (selected ? Resources::FontType::Hi : Resources::FontType::Normal);
    auto font = Resources::font(item.handle->fontname,type,ms*(options ? 0.95f : 1.15f));

    const int  yy        = int(float(H)*(TouchTop+float(row)*TouchRowHeight));
    const int  yc        = yy+rowH/2;
    const int  yText     = yc+font.pixelSize()/2;
    const bool sliderRow = control.handle->type==zenkit::MenuItemType::SLIDER;
    const bool choiceRow = control.handle->type==zenkit::MenuItemType::CHOICEBOX;
    const bool keysRow   = control.handle->type==zenkit::MenuItemType::INPUT &&
                           control.handle->on_chg_set_option_section=="KEYS";

    std::string label = item.handle->text[0];
    if(choiceRow && controlId==index) {
      const auto sep = label.find('#');
      label = sep==std::string::npos ? control.handle->text[1] : label.substr(0,sep);
      }
    if(label.empty()) {
      // scripts often place the label in a separate, non-selectable TEXT item
      int best = 8193;
      for(const auto& candidate:hItems) {
        if(!candidate.handle || !candidate.visible || candidate.handle->type!=zenkit::MenuItemType::TEXT ||
           candidate.handle->text[0].empty() || candidate.handle->pos_x>=control.handle->pos_x)
          continue;
        const int distance = std::abs(candidate.handle->pos_y-control.handle->pos_y);
        if(distance<best && distance<350) {
          best  = distance;
          label = candidate.handle->text[0];
          }
        }
      }
    if(control.handle->type==zenkit::MenuItemType::INPUT && label.empty())
      label = "---";

    const bool hasValue = sliderRow || choiceRow || keysRow;
    if(!hasValue) {
      // a button: centered, the selected one is framed with ornaments
      const int tw = font.textSize(label).w;
      font.drawText(p,midX-tw/2,yText,label);
      if(selected && isEnabled(item.handle)) {
        const int   th  = std::max(2,int(ms*1.6f));
        const int   gap = int(24*ms);
        const int   len = int(float(W)*(options ? 0.07f : 0.09f));
        const float r   = 5.f*ms;
        touchBar(p,midX-tw/2-gap-len,yc,midX-tw/2-gap,th,Gold);
        touchBar(p,midX+tw/2+gap,yc,midX+tw/2+gap+len,th,Gold);
        touchDiamond(p,float(midX-tw/2-gap),float(yc),r,Gold);
        touchDiamond(p,float(midX+tw/2+gap),float(yc),r,Gold);
        }
      if(!options && row+1<TouchRows && size_t(touch.offset+row+1)<rows.size())
        touchDivider(p,int(float(W)*0.37f),int(float(W)*0.63f),yy+rowH,ms,Hairline,3.f);
      continue;
      }

    p.setScissor(int(float(W)*TouchLeft),yy,int(float(W)*0.44f),rowH);
    font.drawText(p,left,yText,label);
    p.setScissor(0,0,W,H);

    // values of the options
    const int x1 = int(float(W)*TouchSliderX);
    const int ww = int(float(W)*TouchSliderW);
    if(sliderRow) {
      const float value = int(controlId)==touch.slider ? touch.preview :
        Gothic::settingsGetF(control.handle->on_chg_set_option_section,control.handle->on_chg_set_option);
      const float v     = std::clamp(value,0.f,1.f);
      const int   th    = std::max(5,int(7*ms));
      p.setBrush(Brush(Color(0.02f,0.03f,0.05f,0.90f)));
      p.drawRect(x1,yc-th/2-1,ww,th+2);
      p.setBrush(Brush(Gold));
      p.drawRect(x1,yc-th/2,int(float(ww)*v),th);
      touchDiamond(p,float(x1)+float(ww)*v,float(yc),11.f*ms,Color(0.98f,0.86f,0.50f,1.f));
      } else
    if(keysRow) {
      auto value = KeyCodec::keysStr(Gothic::settingsGetS(control.handle->on_chg_set_option_section,
                                                          control.handle->on_chg_set_option));
      font.drawText(p,x1,yText,value);
      } else
    if(choiceRow) {
      getText(control,textBuf);
      const std::string value = textBuf.data();
      const bool        toggle = strEnumSize(control.handle->text[0])==2;
      const int         ph     = int(float(rowH)*0.66f);
      const int         px     = x1-int(6*ms);
      const int         pw     = toggle ? int(float(W)*0.17f) : ww+int(12*ms);
      const int         th     = std::max(2,int(2*ms));
      touchFrame(p,px,yc-ph/2,pw,ph,th,GoldSoft,Color(0.02f,0.03f,0.05f,0.82f));

      const int avail = pw-int(70*ms);
      auto valueFont  = font;
      if(font.textSize(value).w>avail && avail>0)
        valueFont.setScale(ms*0.95f*float(avail)/float(font.textSize(value).w));
      if(toggle) {
        const int  r    = ph/2-th-int(3*ms);
        const bool on   = Gothic::settingsGetI(control.handle->on_chg_set_option_section,control.handle->on_chg_set_option)!=0;
        const int  knob = on ? px+pw-r-th-int(5*ms) : px+r+th+int(5*ms);
        valueFont.drawText(p,on ? px+int(14*ms) : px+pw-int(14*ms)-valueFont.textSize(value).w,yText,value);
        touchDiamond(p,float(knob),float(yc),float(r)*1.25f,Color(0.95f,0.80f,0.45f,1.f));
        } else {
        valueFont.drawText(p,px+pw/2-valueFont.textSize(value).w/2,yText,value);
        font.drawText(p,px+int(14*ms),yText,"<");
        font.drawText(p,px+pw-int(14*ms)-font.textSize(">").w,yText,">");
        }
      }
    touchDivider(p,left,int(float(W)*0.88f),yy+rowH,ms,Hairline,3.f);
    }

  // description of the selected button and the version
  if(!options) {
    if(auto sel=selectedItem()) {
      const auto& h = sel->handle;
      if(h->type==zenkit::MenuItemType::BUTTON && !h->text[1].empty()) {
        auto small = Resources::font(Resources::FontType::Normal,ms*0.85f);
        const std::string& txt = h->text[1];
        small.drawText(p,midX-small.textSize(txt).w/2,int(float(H)*0.935f),txt);
        touchDivider(p,int(float(W)*0.33f),int(float(W)*0.67f),int(float(H)*0.895f),ms,Gold,5.f);
        }
      }
    }
  if(owner.hasVersionLine()) {
    auto small = Resources::font(Resources::FontType::Disabled,ms*0.8f);
    small.drawText(p,midX-small.textSize(appBuild).w/2,int(float(H)*0.975f),appBuild);
    }

  if(rows.size()>size_t(TouchRows)) {
    auto small = Resources::font(ms);
    const int last = std::min(touch.offset+TouchRows,int(rows.size()));
    string_frm page(touch.offset+1," - ",last," / ",int(rows.size()));
    small.drawText(p,left,int(float(H)*0.975f),page);
    }
  }
/// The frame of the original menu, as large as the screen allows
Tempest::Rect GameMenu::touchFrameRect() const {
  float fw = float(w())*0.96f;
  float fh = float(h())*0.96f;
  const auto orig = originalSize();
  if(orig.w>0 && orig.h>0) {
    const float aspect = float(orig.w)/float(orig.h);
    if(fw/fh>aspect)
      fw = fh*aspect; else
      fh = fw/aspect;
    }
  return Tempest::Rect(int((float(w())-fw)/2),int((float(h())-fh)/2),int(fw),int(fh));
  }

void GameMenu::paintTouchFrame(Painter& p, const Tempest::Rect& r) {
  p.setBrush(Brush(Color(0.f,0.f,0.f,0.38f)));
  p.drawRect(0,0,w(),h());
  if(back!=nullptr) {
    p.setBrush(*back);
    p.drawRect(r.x,r.y,r.w,r.h,0,0,back->w(),back->h());
    } else {
    touchFrame(p,r.x,r.y,r.w,r.h,3,Color(0.45f,0.45f,0.47f,1.f),Color(0.03f,0.03f,0.035f,1.f));
    }
  }

/// The journal for a phone, in the frame of the original one: sections on the left, quests or their notes on the right
GameMenu::LogView GameMenu::touchLogView() const {
  LogView v;
  v.frame = touchFrameRect();
  const auto& f = v.frame;
  auto fx = [&](float n) { return f.x+int(float(f.w)*n); };
  auto fy = [&](float n) { return f.y+int(float(f.h)*n); };

  // slots of the sections are drawn by the picture of the menu
  for(int i=0; i<4; ++i)
    v.tab[i] = Tempest::Rect(fx(0.045f),fy(0.150f+0.1255f*float(i)),int(float(f.w)*0.317f),int(float(f.h)*0.1255f));

  v.area = Tempest::Rect(fx(0.380f),fy(0.070f),int(float(f.w)*0.560f),int(float(f.h)*0.845f));
  v.back = Tempest::Rect(v.area.x,v.area.y,int(float(f.h)*0.085f),int(float(f.h)*0.085f));
  return v;
  }

std::vector<const QuestLog::Quest*> GameMenu::touchLogQuests() const {
  static const QuestStat tabs[4] = {QuestStat::Current, QuestStat::Old, QuestStat::Failed, QuestStat::Log};
  std::vector<const QuestLog::Quest*> ret;
  if(auto ql = Gothic::inst().questLog()) {
    for(size_t i=0; i<ql->questCount(); ++i) {
      auto& quest = ql->quest(ql->questCount()-i-1); // the newest is the first
      if(isCompatible(quest,tabs[std::clamp(touchLog.tab,0,3)]))
        ret.push_back(&quest);
      }
    }
  return ret;
  }

/// Names of the sections are taken from the buttons of the menu, in the order of the screen
std::array<std::string,4> GameMenu::touchLogTabNames() const {
  std::array<std::string,4> names = {"Current","Finished","Failed","Info"};

  // the sections are the selectable texts of the left column
  auto collect = [&](bool selectableOnly) {
    std::vector<const Item*> found;
    for(const auto& it:hItems) {
      if(!it.handle || !it.visible || isHidden(it.handle) || it.handle->text[0].empty())
        continue;
      if(it.handle->type==zenkit::MenuItemType::LISTBOX || it.handle->type==zenkit::MenuItemType::SLIDER)
        continue;
      if(it.handle->pos_x>3600 || it.handle->pos_y<600 || it.handle->pos_y>5600)
        continue;
      if(selectableOnly && !isSelectable(it.handle))
        continue;
      found.push_back(&it);
      }
    std::sort(found.begin(),found.end(),[](const Item* a, const Item* b){
      return a->handle->pos_y<b->handle->pos_y;
      });
    return found;
    };

  auto found = collect(true);
  if(found.size()<4)
    found = collect(false);
  if(found.size()>=4) {
    for(size_t i=0; i<4; ++i)
      names[i] = found[i]->handle->text[0];
    }
  return names;
  }

void GameMenu::mobileLogTouch(float px, float py, int phase) {
  const auto v = touchLogView();
  if(phase==0) {
    touchLog.drag   = 0;
    touchLog.startY = py;
    if(v.area.contains(int(px),int(py))) {
      touchLog.drag        = touchLog.notes ? 2 : 1;
      touchLog.startScroll = touchLog.notes ? touchLog.textScroll : touchLog.listScroll;
      }
    return;
    }
  if(phase>=2 || touchLog.drag==0) {
    touchLog.drag = 0;
    return;
    }

  // a finger that only trembles is a tap, not a scroll
  const float dead = float(h())*0.012f;
  float dy = touchLog.startY-py;
  dy = dy>dead ? dy-dead : dy<-dead ? dy+dead : 0.f;

  if(touchLog.drag==1)
    touchLog.listScroll = std::clamp(touchLog.startScroll+dy,0.f,float(touchLog.listMax)); else
    touchLog.textScroll = std::clamp(touchLog.startScroll+dy,0.f,float(touchLog.textMax));
  update();
  }

void GameMenu::mobileLogTap(float px, float py) {
  const auto v = touchLogView();
  for(int i=0; i<4; ++i) {
    if(!v.tab[i].contains(int(px),int(py)))
      continue;
    touchLog.tab        = i;
    touchLog.sel        = 0;
    touchLog.notes      = false;
    touchLog.listScroll = 0;
    touchLog.textScroll = 0;
    Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_BROWSE"));
    update();
    return;
    }

  if(touchLog.notes) {
    if(v.back.contains(int(px),int(py))) {
      touchLog.notes = false;
      Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_BROWSE"));
      update();
      }
    return;
    }

  if(v.area.contains(int(px),int(py))) {
    const int at = int(py)-v.area.y+int(touchLog.listScroll);
    for(size_t i=0; i<touchLog.rows.size(); ++i) {
      if(at<touchLog.rows[i].first || at>=touchLog.rows[i].first+touchLog.rows[i].second)
        continue;
      touchLog.sel        = int(i);
      touchLog.notes      = true;
      touchLog.textScroll = 0;
      Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_SELECT"));
      update();
      return;
      }
    }
  }

void GameMenu::paintTouchLog(Painter& p) {
  const auto  v  = touchLogView();
  const auto& f  = v.frame;
  const float ms = std::min(float(f.w)/1000.f,float(f.h)/700.f)*Gothic::interfaceScale(this);
  const int   W  = w();
  const int   H  = h();

  paintTouchFrame(p,f);

  // sections: the text is in the middle of its slot, the chosen one is bright
  const auto names = touchLogTabNames();
  for(int i=0; i<4; ++i) {
    auto font = Resources::font(i==touchLog.tab ? Resources::FontType::Hi : Resources::FontType::Normal,ms*1.15f);
    const auto& r  = v.tab[i];
    const int   pw = r.w-int(24*ms);
    const int   th = font.textSize(pw,names[size_t(i)]).h;
    font.drawText(p,r.x+int(12*ms),r.y+(r.h-th)/2+font.pixelSize(),pw,0,names[size_t(i)],Tempest::AlignHCenter);
    if(i==touchLog.tab) {
      const int   tw = font.textSize(pw,names[size_t(i)]).w;
      const float cy = float(r.y)+float(r.h)*0.5f;
      const float d  = 4.5f*ms;
      touchDiamond(p,float(r.x+r.w/2-tw/2)-14*ms,cy,d,Gold);
      touchDiamond(p,float(r.x+r.w/2+tw/2)+14*ms,cy,d,Gold);
      }
    }

  // day and time: the texts of the menu on the left, as the original has put them
  {
    auto fnt = Resources::font(Resources::FontType::Normal,ms*0.95f);
    for(const auto& it:hItems) {
      if(!it.handle || !it.visible || it.handle->type!=zenkit::MenuItemType::TEXT || it.handle->text[0].empty())
        continue;
      if(it.handle->pos_x>3000 || it.handle->pos_y<5000)
        continue;
      const int x = f.x+int(float(f.w)*float(it.handle->pos_x)/8192.f);
      const int y = f.y+int(float(f.h)*float(it.handle->pos_y)/8192.f);
      fnt.drawText(p,x,y+fnt.pixelSize(),it.handle->text[0]);
      }
  }

  const auto quests = touchLogQuests();
  touchLog.sel = std::clamp(touchLog.sel,0,std::max(0,int(quests.size())-1));
  if(quests.empty())
    touchLog.notes = false;

  const auto& a   = v.area;
  const int   pad = int(12*ms);

  if(!touchLog.notes) {
    // list of the quests
    auto fnt = Resources::font(Resources::FontType::Normal,ms*1.05f);
    touchLog.rows.clear();
    int y = 0;
    for(auto* q:quests) {
      const int th = fnt.textSize(a.w-2*pad,q->name).h;
      const int rh = std::max(th+2*pad,int(float(f.h)*0.085f));
      touchLog.rows.emplace_back(y,rh);
      y += rh;
      }
    touchLog.listMax    = std::max(0,y-a.h);
    touchLog.listScroll = std::clamp(touchLog.listScroll,0.f,float(touchLog.listMax));

    p.setScissor(a.x,a.y,a.w,a.h);
    for(size_t i=0; i<quests.size(); ++i) {
      const int ry = a.y+touchLog.rows[i].first-int(touchLog.listScroll);
      const int rh = touchLog.rows[i].second;
      if(ry+rh<a.y || ry>a.y+a.h)
        continue;
      const int th = fnt.textSize(a.w-2*pad,quests[i]->name).h;
      fnt.drawText(p,a.x+pad,ry+(rh-th)/2+fnt.pixelSize(),a.w-2*pad,0,quests[i]->name,Tempest::AlignLeft);
      touchBar(p,a.x+pad,ry+rh,a.x+a.w-pad,1,Hairline);
      }
    p.setScissor(0,0,W,H);

    if(touchLog.listMax>0) {
      const int bw = std::max(3,int(3*ms));
      const int th = std::max(bw*4,int(float(a.h)*float(a.h)/float(a.h+touchLog.listMax)));
      const int ty = a.y+int(float(a.h-th)*touchLog.listScroll/float(touchLog.listMax));
      p.setBrush(Brush(Color(0.86f,0.68f,0.30f,0.75f)));
      p.drawRect(a.x+a.w-bw-2,ty,bw,th);
      }
    return;
    }

  // notes of the quest
  auto head = Resources::font(Resources::FontType::Hi,ms*1.1f);
  auto fnt  = Resources::font(Resources::FontType::Normal,ms*0.95f);
  auto& q   = *quests[size_t(touchLog.sel)];

  // the way back to the list
  {
    const auto& b = v.back;
    const float r  = float(b.w)*0.18f;
    const float cx = float(b.x)+float(b.w)*0.5f;
    const float cy = float(b.y)+float(b.h)*0.5f;
    const float th = std::max(2.f,2.5f*ms);
    touchStroke(p,cx+r*0.6f,cy-r*1.5f,cx-r*0.8f,cy,th,Gold);
    touchStroke(p,cx-r*0.8f,cy,cx+r*0.6f,cy+r*1.5f,th,Gold);
    }

  const int hx = a.x+v.back.w;
  const int hw = a.w-v.back.w-pad;
  const int hh = std::max(head.textSize(hw,q.name).h,v.back.h);
  head.drawText(p,hx,a.y+(hh-head.textSize(hw,q.name).h)/2+head.pixelSize(),hw,0,q.name,Tempest::AlignLeft);
  touchDivider(p,a.x+pad,a.x+a.w-pad,a.y+hh+int(4*ms),ms,Gold,4.f);

  const Tempest::Rect view(a.x,a.y+hh+int(12*ms),a.w,a.h-hh-int(12*ms));
  const int tw = view.w-2*pad;

  int total = pad;
  for(auto& e:q.entry)
    total += fnt.textSize(tw,e).h+pad*2;
  touchLog.textMax    = std::max(0,total-view.h);
  touchLog.textScroll = std::clamp(touchLog.textScroll,0.f,float(touchLog.textMax));

  p.setScissor(view.x,view.y,view.w,view.h);
  int y = view.y+pad-int(touchLog.textScroll);
  for(auto& e:q.entry) {
    fnt.drawText(p,view.x+pad,y+fnt.pixelSize(),tw,0,e,Tempest::AlignLeft);
    y += fnt.textSize(tw,e).h+pad;
    touchBar(p,view.x+pad,y,view.x+view.w-pad,1,Hairline);
    y += pad;
    }
  p.setScissor(0,0,W,H);

  if(touchLog.textMax>0) {
    const int bw = std::max(3,int(3*ms));
    const int th = std::max(bw*4,int(float(view.h)*float(view.h)/float(view.h+touchLog.textMax)));
    const int ty = view.y+int(float(view.h-th)*touchLog.textScroll/float(touchLog.textMax));
    p.setBrush(Brush(Color(0.86f,0.68f,0.30f,0.75f)));
    p.drawRect(view.x+view.w-bw-2,ty,bw,th);
    }
  }

#endif

void GameMenu::paintEvent(PaintEvent &e) {
  const float scale = fontScale();

  Painter p(e);
  p.setScissor(-x(),-y(),owner.w(),owner.h());
#ifdef OPENGOTHIC_TOUCH_UI
  if(touch.log) {
    paintTouchLog(p);
    return;
    }
  if(touch.list) {
    paintTouch(p);
    return;
    }
#endif

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

void GameMenu::drawItem(Painter& p, Item& hItem) {
  if(!hItem.visible || hItem.name.empty())
    return;
  if(isHidden(hItem.handle))
    return;

  auto& item  = hItem.handle;
  auto flags = item->flags;
  getText(hItem,textBuf);

  const auto rect = itemRect(hItem);
  const int x = rect.x, y = rect.y, szX = rect.w, szY = rect.h;

  if(hItem.img && !hItem.img->isEmpty()) {
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

  const float scale = fontScale();
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

Rect GameMenu::itemRect(const Item& it) const {
  const auto& item = it.handle;
  const int dimx = item->dim_x!=-1 ? item->dim_x : 8192;
  const int dimy = item->dim_y!=-1 ? item->dim_y : 750;
  return Rect(int(float(w()*item->pos_x)/scriptDiv),int(float(h()*item->pos_y)/scriptDiv),
              int(float(w()*dimx)/scriptDiv),int(float(h()*dimy)/scriptDiv));
  }

GameMenu::Item* GameMenu::touchHitTest(Item& p, Point pos) {
  auto* it = &p;
  while(it!=nullptr) {
    auto rect = itemRect(*it);
    if(rect.contains(pos))
      return it;
    if(it->handle->flags & zenkit::MenuItemFlag::EFFECTS) {
      auto next=selectedNextItem(it);
      if(next!=&p)
        it=next;
      } else {
      it=nullptr;
      }
    }
  return nullptr;
  }

void GameMenu::onBeginTouch(Point pos) {
  touchSlider = nullptr;
  touchActive = false;

  // reverse of draw order
  for(uint32_t i = zenkit::IMenu::item_count; i-- > 0;) {
    if(hItems[i].handle==nullptr)
      continue;
    auto& item = hItems[i].handle;
    if(!isEnabled(item) || isHidden(item))
      continue;
    if(!isSelectable(item))
      continue;
    auto hit = touchHitTest(hItems[i], pos);
    if(hit==nullptr)
      continue;
    setSelection(int(i), -1);
    if(auto s = selectedItem()) {
      updateSavThumb(*s);
      touchActive = true;
      }
    if(hit->handle->type==zenkit::MenuItemType::SLIDER) {
      touchSlider = hit;
      break;
      }
    break;
    }
  }

void GameMenu::onDragTouch(Point pos) {
  if(touchSlider==nullptr)
    return;
  const auto  rect = itemRect(*touchSlider);
  const auto& sec  = touchSlider->handle->on_chg_set_option_section;
  const auto& opt  = touchSlider->handle->on_chg_set_option;
  if(rect.w<=0 || sec.empty() || opt.empty())
    return;

  float value = std::clamp(float(pos.x-rect.x)/float(rect.w),0.f,1.f) * 100.f;
  float stp   = sliderStep(touchSlider->handle) * 100.f;
  if(stp<=1.f)
    stp = 1.f;

  value = std::clamp(std::round(value/stp)*stp,0.f,100.f);

  Gothic::settingsSetF(sec,opt,value/100.f);
  update();
  }

void GameMenu::onEndTouch(bool activate) {
  const bool tap = touchActive && touchSlider==nullptr;
  touchSlider = nullptr;
  touchActive = false;
  if(activate && tap)
    onKeyboard(KeyCodec::ActionGeneric);
  }

void GameMenu::onKeyboard(KeyCodec::Action key) {
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

#ifdef OPENGOTHIC_TOUCH_UI
  if(touch.list || touch.log) {
    resize(owner.size());
    setPosition(0,0);
    return;
    }

  {
    const auto orig = originalSize();
    if(orig.w>0 && orig.h>0) {
      touchK = std::min(0.96f*float(owner.w())/float(orig.w),0.96f*float(owner.h())/float(orig.h));
      resize(int(float(orig.w)*touchK),int(float(orig.h)*touchK));
      setPosition((owner.w()-w())/2,(owner.h()-h())/2);
      return;
      }
  }
#endif
  const float scale = Gothic::interfaceScale(this);
  const float fx    = 640.0f * scale;
  const float fy    = 480.0f * scale;

  const float wx = float(owner.w());
  const float wy = float(owner.h());

  Size size = {0, 0};
  if(menu->flags & zenkit::MenuFlag::DONT_SCALE_DIMENSION) {
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

float GameMenu::fontScale() const {
  return Gothic::interfaceScale(this)*touchK;
  }

/// Size of the menu as the original interface shows it
Tempest::Size GameMenu::originalSize() const {
  const float scale = Gothic::interfaceScale(this);
  const float fx    = 640.0f * scale;
  const float fy    = 480.0f * scale;
  const float wx    = float(owner.w());
  const float wy    = float(owner.h());
  if(menu->flags & zenkit::MenuFlag::DONT_SCALE_DIMENSION)
    return {int(float(menu->dim_x)*fx/scriptDiv),int(float(menu->dim_y)*fy/scriptDiv)};
  return {int(float(menu->dim_x)*wx/scriptDiv),int(float(menu->dim_y)*wy/scriptDiv)};
  }

const GthFont& GameMenu::getTextFont(const GameMenu::Item &it) {
  const float scale = fontScale();
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

float GameMenu::sliderStep(const std::shared_ptr<zenkit::IMenuItem>& item) {
  if(item==nullptr || item->type!=zenkit::MenuItemType::SLIDER)
    return 0;
  float v = item->user_float[0];
  if(v==0.f)
    return 0.03f; // error?
  return 1.f/v;
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
    float s = sliderStep(item.handle);
    v  = std::max(0.f,std::min(v+float(slideDx)*s,1.f));
    Gothic::settingsSetF(sec, opt, v);
    }
  if(item.handle->type==zenkit::MenuItemType::CHOICEBOX) {
    updateItem(item);
    const int cnt = int(strEnumSize(item.handle->text[0]));
#if defined(__MOBILE_PLATFORM__)
    if(slideDx==0 && cnt>0)
      item.value = (item.value+1)%cnt; else
      item.value += slideDx;
#else
    if(slideDx==0 && cnt==2)
      slideDx = 1;
    item.value += slideDx;
#endif
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
