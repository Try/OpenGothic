#include "inventorymenu.h"
#include "ui/cursorshape.h"

#include <Tempest/Painter>
#include <Tempest/SoundEffect>

#include "utils/string_frm.h"
#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "utils/gthfont.h"
#include "utils/keycodec.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

struct InventoryMenu::Page {
  Page()=default;
  Page(const Page&)=delete;
  virtual ~Page()=default;

  Category category = CatAll;

  bool matches(const Item& item) const {
    if(category==CatAll)
      return true;

    const uint32_t flags   = uint32_t(item.mainFlag());
    const bool     weapons = (flags & (ITM_CAT_NF | ITM_CAT_FF | ITM_CAT_MUN))!=0;
    const bool     armor   = item.isArmor();
    const bool     jewelry = item.isRing() || (uint32_t(item.itemFlag()) & ITM_AMULET)!=0;
    const bool     magic   = item.isSpellOrRune();
    const bool     food    = (flags & ITM_CAT_FOOD)!=0;
    const bool     potion  = (flags & ITM_CAT_POTION)!=0;
    const bool     docs    = (flags & ITM_CAT_DOCS)!=0;
    switch(category) {
      case CatWeapons:   return weapons;
      case CatArmor:     return armor;
      case CatJewelry:   return jewelry;
      case CatMagic:     return magic;
      case CatFood:      return food;
      case CatPotion:    return potion;
      case CatDocuments: return docs;
      case CatMisc:      return !weapons && !armor && !jewelry && !magic && !food && !potion && !docs;
      default:           return true;
      }
    }

  /// Iterators below skip items that do not belong to the current category
  Inventory::Iterator first() const {
    auto it = iterator();
    while(it.isValid() && !matches(*it))
      ++it;
    return it;
    }

  void next(Inventory::Iterator& it) const {
    if(it.isValid())
      ++it;
    while(it.isValid() && !matches(*it))
      ++it;
    }

  size_t size() const {
    if(is(nullptr))
      return 0;
    size_t ret = 0;
    for(auto it = first(); it.isValid(); next(it))
      ret++;
    return ret;
    }

  Inventory::Iterator get(size_t id) const {
    auto it = first();
    for(size_t i=0; i<id && it.isValid(); ++i)
      next(it);
    return it;
    }

  virtual bool                is(const Inventory* i) const { return i==nullptr; }
  virtual Inventory::Iterator iterator() const { throw std::runtime_error("index out of range");  }
  };

struct InventoryMenu::InvPage : InventoryMenu::Page {
  InvPage(const Inventory& i):inv(i){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Inventory);
    }

  const Inventory& inv;
  };

struct InventoryMenu::TradePage : InventoryMenu::Page {
  TradePage(const Inventory& i):inv(i){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Trade);
    }

  const Inventory& inv;
  };

struct InventoryMenu::RansackPage : InventoryMenu::Page {
  RansackPage(const Inventory& i):inv(i){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Ransack);
    }

  const Inventory& inv;
  };

InventoryMenu::InventoryMenu(const KeyCodec& key)
  :keycodec(key) {
  slot = Resources::loadTexture("INV_SLOT.TGA");
  selT = Resources::loadTexture("INV_SLOT_HIGHLIGHTED.TGA");
  selU = Resources::loadTexture("INV_SLOT_EQUIPPED.TGA");
  tex  = Resources::loadTexture("INV_BACK.TGA"); // INV_TITEL.TGA

  int invMaxColumns = Gothic::settingsGetI("GAME","invMaxColumns");
  if(invMaxColumns>0)
    columsCount = size_t(invMaxColumns); else
    columsCount = 5;
#ifdef OPENGOTHIC_TOUCH_UI
  columsCount = TouchColumns;
#endif

  setFocusPolicy(NoFocus);
  setCursorShape(HiddenCursor);
  takeTimer.timeout.bind(this,&InventoryMenu::onTakeStuff);
  }

InventoryMenu::~InventoryMenu() {
  }

void InventoryMenu::close() {
  if(state!=State::Closed) {
    if(state==State::Trade)
      Gothic::inst().emitGlobalSound("TRADE_CLOSE"); else
      Gothic::inst().emitGlobalSound("INV_CLOSE");
    }
  renderer.reset(true);
  takeTimer.stop();
  state  = State::Closed;
  }

void InventoryMenu::open(Npc &pl) {
  if(pl.isDown() || pl.isMonster() || pl.isInAir() || pl.isSlide() || (pl.interactive()!=nullptr))
    return;
  if(pl.bodyStateMasked()==BS_UNCONSCIOUS || pl.bodyStateMasked()==BS_LIE)
    return;
  if(pl.weaponState()!=WeaponState::NoWeapon) {
    pl.stopAnim("");
    pl.closeWeapon(false);
    }
  state  = State::Equip;
  player = &pl;
  trader = nullptr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage  (pl.inventory()));
  pageOth.reset();
  adjustScroll();
  update();

  Gothic::inst().emitGlobalSound("INV_OPEN");
  //Gothic::inst().emitGlobalSound("INV_CHANGE");
  }

void InventoryMenu::trade(Npc &pl, Npc &tr) {
  if(pl.isDown())
    return;
  state  = State::Trade;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage  (pl.inventory()));
  pageOth.reset(new TradePage(tr.inventory()));
  adjustScroll();
  update();
  Gothic::inst().emitGlobalSound("TRADE_OPEN");
  }

bool InventoryMenu::ransack(Npc &pl, Npc &tr) {
  if(pl.isDown())
    return false;
  auto it = tr.inventory().iterator(Inventory::T_Ransack);
  if(!it.isValid())
    return false;
  state  = State::Ransack;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage    (pl.inventory()));
  pageOth.reset(new RansackPage(tr.inventory()));
  adjustScroll();
  update();
  Gothic::inst().emitGlobalSound("INV_OPEN");
  return true;
  }

void InventoryMenu::open(Npc &pl, Interactive &ch) {
  if(pl.isDown())
    return;
  const bool needToPicklock = ch.needToLockpick(pl);
  if(!pl.setInteraction(&ch))
    return;

  if(needToPicklock && !ch.isCracked()) {
    state = State::LockPicking;
    } else {
    state = State::Chest;
    }

  player = &pl;
  trader = nullptr;
  chest  = &ch;
  page   = 0;
  pagePl .reset(new InvPage(pl.inventory()));
  pageOth.reset(new InvPage(ch.inventory()));
  adjustScroll();
  update();
  }

InventoryMenu::State InventoryMenu::isOpen() const {
  return state;
  }

bool InventoryMenu::isActive() const {
  return state!=State::Closed;
  }

void InventoryMenu::onWorldChanged() {
  close();
  player = nullptr;
  trader = nullptr;
  chest  = nullptr;
  }

void InventoryMenu::tick(uint64_t /*dt*/) {
  if(player!=nullptr && (player->isDown() || player->bodyStateMasked()==BS_UNCONSCIOUS)) {
    close();
    return;
    }

  if(state==State::LockPicking) {
    if(chest->isCracked()) {
      state = State::Chest;
      return;
      }
    }

  if(state==State::Ransack) {
    if(trader==nullptr){
      close();
      return;
      }

    if(!trader->isDown()) {
      close();
      return;
      }

    auto it = trader->inventory().iterator(Inventory::T_Ransack);
    if(!it.isValid())
      close();
    }

  if(state==State::Closed) {
    if(player!=nullptr){
      if(!player->setInteraction(nullptr))
         return;
      player = nullptr;
      chest  = nullptr;
      }

    page = 0;
    renderer.reset();
    pagePl .reset();
    pageOth.reset();
    update();
    }
  }

void InventoryMenu::processMove(KeyEvent& e) {
  auto key = keycodec.tr(e);
  if(key==KeyCodec::Forward)
    moveUp();
  else if(key==KeyCodec::Back)
    moveDown();
  else if(key==KeyCodec::Left || key==KeyCodec::RotateL)
    moveLeft(true);
  else if(key==KeyCodec::Right || key==KeyCodec::RotateR)
    moveRight(true);
  }

void InventoryMenu::moveLeft(bool usePage) {
  auto& sel = activePageSel();

  if(usePage && sel.sel%columsCount==0 && page>0)
    page--;
  else if(sel.sel>0)
    sel.sel--;
  }

void InventoryMenu::moveRight(bool usePage) {
  auto&        pg     = activePage();
  auto&        sel    = activePageSel();
  const size_t pCount = pagesCount();

  if(usePage && ((sel.sel+1u)%columsCount==0 || sel.sel+1u==pg.size() || pg.size()==0) && page+1u<pCount)
    page++;
  else if(sel.sel+1<pg.size())
    sel.sel++;
  }

void InventoryMenu::moveUp() {
  auto& sel = activePageSel();

  if(sel.sel>=columsCount)
    sel.sel -= columsCount;
  else
    moveLeft(false);
  }

void InventoryMenu::moveDown() {
  auto& pg  = activePage();
  auto& sel = activePageSel();

  if(sel.sel+columsCount<pg.size())
    sel.sel += columsCount;
  else
    moveRight(false);
  }

void InventoryMenu::keyDownEvent(KeyEvent &e) {
  if(state==State::Closed || state==State::LockPicking){
    e.ignore();
    return;
    }

  processMove(e);

  if(keycodec.tr(e)==KeyCodec::Jump) {
    lootMode = LootMode::Stack;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if (keycodec.tr(e)==KeyCodec::ActionGeneric || e.key==KeyEvent::K_Return) {
    onItemAction(Item::NSLOT);
    }
  else if((KeyEvent::K_3<=e.key && e.key<=KeyEvent::K_9) || e.key==KeyEvent::K_0) {
    uint8_t slot = 10;
    if((KeyEvent::K_3<=e.key && e.key<=KeyEvent::K_9))
      slot = uint8_t(e.key-KeyEvent::K_0);
    onItemAction(slot);
    }
  else if(e.key==KeyEvent::K_ESCAPE || keycodec.tr(e)==KeyCodec::Inventory){
    close();
    }
  else if(e.key==KeyEvent::K_Space) {
    lootMode = LootMode::Normal;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if(e.key==KeyEvent::K_Z) {
    lootMode = LootMode::Ten;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if(e.key==KeyEvent::K_X) {
    lootMode = LootMode::Hundred;
    takeTimer.start(200);
    onTakeStuff();
    }

  adjustScroll();
  update();
  }

void InventoryMenu::keyRepeatEvent(KeyEvent& e) {
  if(state==State::LockPicking || state==State::Closed)
    return;
  processMove(e);
  adjustScroll();
  update();
  }

void InventoryMenu::keyUpEvent(KeyEvent&) {
  takeTimer.stop();
  lootMode = LootMode::Normal;
  takeCount = 0;
  }

void InventoryMenu::mouseDownEvent(MouseEvent &e) {
#ifdef OPENGOTHIC_TOUCH_UI
  // Touches are taken by AndroidInput, see mobileTap
  e.ignore();
  return;
#endif
  if(player==nullptr || state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  if (e.button==MouseEvent::ButtonLeft)
    onItemAction(Item::NSLOT);
  else if (e.button==MouseEvent::ButtonRight)
    close();

  adjustScroll();
  }

void InventoryMenu::mouseUpEvent(MouseEvent&) {
  takeTimer.stop();
  takeCount=0;
  }

void InventoryMenu::mouseWheelEvent(MouseEvent &e) {
  if(state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  scrollDelta += e.delta;
  if(scrollDelta>0) {
    for(int i=0;i<scrollDelta/120;++i)
      moveUp();
    scrollDelta %= 120;
    } else {
    for(int i=0;i<-scrollDelta/120;++i)
      moveDown();
    scrollDelta %= 120;
    }
  adjustScroll();
  }

size_t InventoryMenu::rowsCount() const {
#ifdef OPENGOTHIC_TOUCH_UI
  return size_t(Mobile::InventoryLayout::Rows);
#else
  int iy=30+34+70;
  return size_t((h()-iy-infoHeight()-20)/slotSize().h);
#endif
  }

void InventoryMenu::paintEvent(PaintEvent &e) {
  if(player==nullptr || state==State::Closed)
    return;
  renderer.reset();

  Painter p(e);
  drawAll(p,*player,DrawPass::Back);
  }

void InventoryMenu::paintNumOverlay(PaintEvent& e) {
  if(player==nullptr || state==State::Closed)
    return;

  Painter p(e);
  drawAll(p,*player,DrawPass::Front);
  }

Size InventoryMenu::slotSize() const {
#ifdef OPENGOTHIC_TOUCH_UI
  const int side = std::max(40,int(touchLayout().side));
  return Size(side,side);
#else
  const float scale = Gothic::interfaceScale(this);
  const float cell  = float(Gothic::options().inventoryCellSize);
  return Size(int(cell*scale),int(cell*scale));
#endif
  }

#ifdef OPENGOTHIC_TOUCH_UI
float InventoryMenu::touchUnit() const {
  return std::min(float(w())/1000.f,float(h())/500.f);
  }

float InventoryMenu::touchTextScale() const {
  return std::max(0.8f,float(h())/720.f)*Gothic::interfaceScale(this);
  }

Mobile::InventoryLayout InventoryMenu::touchLayout() const {
  return Mobile::inventoryLayout(float(w()),float(h()),pageOth!=nullptr,Gothic::interfaceScale(this));
  }
#endif

int InventoryMenu::infoHeight() const {
  const float scale = Gothic::interfaceScale(this);
  return (Item::MAX_UI_ROWS+2)*int(float(Resources::font(scale).pixelSize()))+int(scale*10)/*padding bottom*/;
  }

size_t InventoryMenu::pagesCount() const {
  if(state==State::Chest || state==State::Trade)
    return 2;
  return 1;
  }

const InventoryMenu::Page &InventoryMenu::activePage() {
  if(pageOth!=nullptr)
    return *(page==0 ? pageOth : pagePl);
  if(pagePl)
    return *pagePl;

  static Page n;
  return n;
  }

InventoryMenu::PageLocal &InventoryMenu::activePageSel() {
  if(pageOth!=nullptr)
    return (page==0 ? pageLocal[0] : pageLocal[1]);
  return pageLocal[1];
  }
  
void InventoryMenu::onItemAction(uint8_t slotHint) {
  auto& page = activePage();
  auto& sel  = activePageSel();

  auto it = page.get(sel.sel);
  if(!it.isValid())
    return;

  if(state==State::Equip) {
    const size_t clsId = it->clsId();
    if(it.isEquipped() && slotHint==Item::NSLOT) {
      player->unequipItem(clsId);
      } else {
      player->useItem(clsId,slotHint,false);
      auto it2 = page.get(sel.sel);
      if((!it2.isValid() || it2->clsId()!=clsId) && sel.sel>0)
        --sel.sel;
      }
    }
  else if(state==State::Chest || state==State::Trade || state==State::Ransack) {
    lootMode = LootMode::Normal;
    takeTimer.start(200);
    onTakeStuff();
    }
  }

void InventoryMenu::onTakeStuff() { 
  size_t itemCount = 0;
  auto& page = activePage();
  auto& sel  = activePageSel();
  if(sel.sel >= page.size())
    return;
  auto it = page.get(sel.sel);
  if(lootMode==LootMode::Normal) {
    ++takeCount;
    itemCount = uint32_t(std::pow(10,takeCount / 10));
    if(it.count() <= itemCount) {
      itemCount = uint32_t(it.count());
      takeCount = 0;
      }
    }
  else if(lootMode==LootMode::Stack) {
    itemCount = it.count();
    }
  else if(lootMode==LootMode::Ten) {
    itemCount = 10;
    }
  else if(lootMode==LootMode::Hundred) {
    itemCount = 100;
    }

  if(it.count() < itemCount) {
    itemCount = it.count();
    }

  if(state==State::Chest) {
    if(page.is(&player->inventory())) {
      player->moveItem(it->clsId(),*chest,itemCount);
      } else {
      player->addItem (it->clsId(),*chest,itemCount);
      }
    }
  else if(state==State::Trade) {
    if(page.is(&player->inventory())) {
      player->sellItem(it->clsId(),*trader,itemCount);
      } else {
      player->buyItem (it->clsId(),*trader,itemCount);
      }
    }
  else if(state==State::Ransack) {
    if(page.is(&trader->inventory())) {
      player->addItem(it->clsId(),*trader,itemCount);
      }
    }
  else if(state==State::Equip) {
    player->dropItem(it->clsId(),itemCount);
    }
  adjustScroll();
  }

void InventoryMenu::adjustScroll() {
  adjustScrollRows();
#ifdef OPENGOTHIC_TOUCH_UI
  // the first visible row follows the selection, the list moves to it
  auto&       sel  = activePageSel();
  const float side = float(std::max(1,slotSize().h));
  if(size_t(sel.scrollPx/side)!=sel.scroll)
    sel.scrollPx = float(sel.scroll)*side;
#endif
  }

void InventoryMenu::adjustScrollRows() {
  auto& page = activePage();
  auto& sel  = activePageSel();
  sel.sel = std::min(sel.sel, std::max<size_t>(page.size(),1)-1);
  while(sel.sel<sel.scroll*columsCount) {
    if(sel.scroll<=1){
      sel.scroll=0;
      return;
      }
    sel.scroll-=1;
    }

  const size_t hcount=rowsCount();
  while(sel.sel>=(sel.scroll+hcount)*columsCount) {
    sel.scroll+=1;
    }
  }

void InventoryMenu::drawAll(Painter &p, Npc &player, DrawPass pass) {
#ifdef OPENGOTHIC_TOUCH_UI
  drawAllTouch(p,player,pass);
  return;
#else
  const int padd    = 43;
  const int iy      = 30+34+70;
  const int headerY = 70;
  const int goldW   = 2*slotSize().w;

  if(state==State::LockPicking)
    return;

  const int wcount = int(columsCount);
  const int hcount = int(rowsCount());

  if(chest!=nullptr){
    if(pass==DrawPass::Back)
      drawHeader(p,chest->displayName(),padd,headerY);
    drawItems(p,pass,*pageOth,pageLocal[0],padd,iy,wcount,hcount);
    }

  if(trader!=nullptr) {
    if(pass==DrawPass::Back)
      drawHeader(p,trader->displayName(),padd,headerY);
    drawItems(p,pass,*pageOth,pageLocal[0],padd,iy,wcount,hcount);
    }

  if(state!=State::Ransack) {
    if(pass==DrawPass::Back)
      drawGold (p,player,w()-padd-goldW,headerY);
    drawItems(p,pass,*pagePl,pageLocal[1],w()-padd-wcount*slotSize().w,iy,wcount,hcount);
    }

  if(pass==DrawPass::Back)
    drawInfo(p);
#endif
  }

void InventoryMenu::drawItems(Painter &p, DrawPass pass,
                              const Page &inv, const PageLocal& sel, int x0, int y, int wcount, int hcount) {
  if(state==State::LockPicking)
    return;

#ifndef OPENGOTHIC_TOUCH_UI
  if(tex!=nullptr && pass==DrawPass::Back) {
    p.setBrush(*tex);
    p.drawRect(x0,y,slotSize().w*wcount,slotSize().h*hcount,
               0,0,tex->w(),tex->h());
    }
#endif

  int rowsToDraw = hcount;
#ifdef OPENGOTHIC_TOUCH_UI
  // the list is moved by pixels: the first row is cut, one more row shows below
  const int firstRow = int(sel.scrollPx)/std::max(1,slotSize().h);
  y -= int(sel.scrollPx)-firstRow*slotSize().h;
  rowsToDraw = hcount+1;
  touchClip  = Tempest::Rect(x0,y+(int(sel.scrollPx)-firstRow*slotSize().h),slotSize().w*wcount,slotSize().h*hcount);
  p.setScissor(touchClip.x,touchClip.y,touchClip.w,touchClip.h);
#else
  const int firstRow = int(sel.scroll);
#endif
  auto   it = inv.first();
  size_t id = 0;
  for(size_t i=0; it.isValid() && i<size_t(firstRow)*size_t(wcount); ++i) {
    inv.next(it);
    ++id;
    }
  for(int i=0;i<rowsToDraw;++i) {
    for(int r=0;r<wcount;++r) {
      const int x = x0 + r*slotSize().w;
      if(pass==DrawPass::Back) {
#ifdef OPENGOTHIC_TOUCH_UI
        p.setBrush(Brush(Color(0.01f,0.018f,0.025f,0.90f)));
        p.drawRect(x+1,y+1,slotSize().w-2,slotSize().h-2);
        p.setBrush(Brush(touchSlot));
        p.drawRect(float(x),float(y),float(slotSize().w),float(slotSize().h),0.f,0.f,float(touchSlot.w()),float(touchSlot.h()));
#else
        p.setBrush(*slot);
        p.drawRect(x,y,slotSize().w,slotSize().h,
                   0,0,slot->w(),slot->h());
#endif
        }
      if(it.isValid()) {
        drawSlot(p,pass, it,inv,sel, x,y, id);
        inv.next(it);
        ++id;
        }
      }
    y += slotSize().h;
    }
#ifdef OPENGOTHIC_TOUCH_UI
  p.setScissor(0,0,w(),h());
#endif
  }

void InventoryMenu::drawSlot(Painter &p, DrawPass pass, const Inventory::Iterator &it,
                             const Page& page, const PageLocal &sel,
                             int x, int y, size_t id) {
  if(!slot)
    return;

  auto& active = activePage();
#ifdef OPENGOTHIC_TOUCH_UI
  const float scale = touchTextScale();
#else
  const float scale = Gothic::interfaceScale(this);
#endif

  if(pass==DrawPass::Back) {
    if((!it.isValid() && id==0) || (id==sel.sel && &page==&active)){
#ifdef OPENGOTHIC_TOUCH_UI
      p.setBrush(Brush(touchSelected));
      p.drawRect(float(x),float(y),float(slotSize().w),float(slotSize().h),0.f,0.f,float(touchSelected.w()),float(touchSelected.h()));
#else
      p.setBrush(*selT);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selT->w(),selT->h());
#endif
      }

    if(it.isEquipped() && selU!=nullptr) {
      p.setBrush(*selU);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selU->w(),selU->h());
      }

    const int dsz = (id==sel.sel ? 5 : 0);
#ifdef OPENGOTHIC_TOUCH_UI
    renderer.drawItem(x-dsz, y-dsz, slotSize().w+2*dsz, slotSize().h+2*dsz, *it, &touchClip);
#else
    renderer.drawItem(x-dsz, y-dsz, slotSize().w+2*dsz, slotSize().h+2*dsz, *it);
#endif
    } else {
    auto fnt = Resources::font(scale);

    if(it.count()>1) {
      string_frm vint(int(it.count()));
      auto sz = fnt.textSize(vint);
      fnt.drawText(p,x+slotSize().w-sz.w-10,
                   y+slotSize().h-10,
                   vint);
      }

    if(it.slot()!=Item::NSLOT) {
      fnt = Resources::font(Resources::FontType::Red, scale);

      string_frm vint(int(it.slot()));
      auto sz = fnt.textSize(vint);
      fnt.drawText(p,x+10,
                   y+slotSize().h/2+sz.h/2,
                   vint);
      }
    }
  }

void InventoryMenu::drawGold(Painter &p, Npc &player, int x, int y) {
  if(!slot)
    return;
  auto           w    = Gothic::inst().world();
  auto           txt  = w ? w->script().currencyName() : "";
  const size_t   gold = player.inventory().goldCount();
  if(txt.empty())
    txt="Gold";

  string_frm vint(txt," : ",int(gold));
#ifdef OPENGOTHIC_TOUCH_UI
  (void)x;
  (void)y;
  drawTouchHeader(p,vint,touchLayout().header[1]);
#else
  drawHeader(p,vint,x,y);
#endif
  }

void InventoryMenu::drawHeader(Painter &p, std::string_view title, int x, int y) {
#ifdef OPENGOTHIC_TOUCH_UI
  const float scale = touchUnit();
#else
  const float scale = Gothic::interfaceScale(this);
#endif
  auto&       fnt   = Resources::font(scale);

  const int   tw    = fnt.textSize(title).w;
  const int   th    = fnt.textSize(title).h;
  const int   padd  = int(8*scale);
  const int   dw    = std::max(slotSize().w*2, tw+padd*2);
  const int   dh    = int(34*scale);

  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh, 0,0,tex->w(),tex->h());
    }
  if(slot) {
    p.setBrush(*slot);
    p.drawRect(x,y,dw,dh, 0,0,slot->w(),slot->h());
    }

  fnt.drawText(p,x+(dw-tw)/2,y+dh/2+th/2,title);
  }

void InventoryMenu::drawInfo(Painter &p) {
#ifdef OPENGOTHIC_TOUCH_UI
  drawInfoTouch(p);
  return;
#else
  const float scale = Gothic::interfaceScale(this);
  const int   dh    = infoHeight();
  const int   dw    = std::min(w(), int(720*scale));
  const int   x     = (w()-dw)/2;
  const int   y     = h()-dh-20;

  auto& pg  = activePage();
  auto& sel = activePageSel();

  auto it = pg.get(sel.sel);
  if(!it.isValid())
    return;

  auto& r = *pg.get(sel.sel);
  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh,
               0,0,tex->w(),tex->h());
    }

  auto& fnt = Resources::font(scale);
  auto  desc = r.description();
  int   tw   = fnt.textSize(desc).w;

  fnt.drawText(p,x+(dw-tw)/2,y+int(fnt.pixelSize()),desc);

  for(size_t i=0;i<Item::MAX_UI_ROWS;++i){
    auto    txt = r.uiText(i);
    int32_t val = r.uiValue(i);

    if(txt.empty())
      continue;

    if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg.is(&player->inventory())){
      val = r.sellCost();
      }

    string_frm vint(val);
    int tw = fnt.textSize(vint).w;

    fnt.drawText(p, x+20,  y+int(i+2)*fnt.pixelSize(), txt);
    if(val!=0)
      fnt.drawText(p,x+dw-tw-20,y+int(i+2)*fnt.pixelSize(),vint);
    }

  const int sz = dh;
  renderer.drawItem(x+dw-sz-sz/2,y,sz,sz,r);
#endif
  }

void InventoryMenu::draw(Tempest::Encoder<CommandBuffer>& cmd) {
  renderer.draw(cmd);
  }

#ifdef OPENGOTHIC_TOUCH_UI
bool InventoryMenu::touchHit(float px, float py, uint8_t& list, int& column, int& row) const {
  const auto lay = touchLayout();
  for(uint8_t i=0; i<2; ++i) {
    if((i==1 && state==State::Ransack) || (i==0 && pageOth==nullptr))
      continue;
    const auto& r = lay.list[i];
    if(!r.contains(px,py))
      continue;
    list   = i;
    column = std::min(int((px-r.x)/lay.side),Mobile::InventoryLayout::Columns-1);
    row    = int((py-r.y+pageLocal[i].scrollPx)/lay.side);   // of the whole list
    return true;
    }
  return false;
  }

void InventoryMenu::mobileTap(float px, float py) {
  if(player==nullptr || state==State::Closed || state==State::LockPicking)
    return;

  uint8_t list   = 0;
  int     column = 0, row = 0;
  if(!touchHit(px,py,list,column,row))
    return;

  Page*      selected = list==1 ? pagePl.get() : pageOth.get();
  PageLocal* local    = &pageLocal[list];
  page = list==1 ? (pageOth ? 1 : 0) : 0;

  const size_t id = size_t(row)*TouchColumns+size_t(column);
  if(selected==nullptr || id>=selected->size())
    return;
  local->sel = id;
  adjustScroll();
  update();
  }

InventoryMenu::PageLocal* InventoryMenu::touchList(float px, float py, const Page*& list, uint8_t& index) {
  int column = 0, row = 0;
  if(!touchHit(px,py,index,column,row))
    return nullptr;
  list = index==1 ? pagePl.get() : pageOth.get();
  return &pageLocal[index];
  }

float InventoryMenu::touchMaxScroll(const Page& list) const {
  const size_t rows = (list.size()+TouchColumns-1)/TouchColumns;
  const size_t view = rowsCount();
  return rows>view ? float(rows-view)*float(slotSize().h) : 0.f;
  }

/// The list under the finger follows it. Phases: 0 - down, 1 - move, 2 - up, 3 - cancelled.
void InventoryMenu::mobileTouch(float x, float y, int phase) {
  if(player==nullptr || state==State::Closed || state==State::LockPicking) {
    touchDrag = TouchDrag();
    return;
    }

  if(phase==0) {
    const Page* list  = nullptr;
    uint8_t     index = 0;
    auto*       local = touchList(x,y,list,index);
    touchDrag = TouchDrag();
    if(local==nullptr)
      return;
    touchDrag.active      = true;
    touchDrag.list        = index;
    touchDrag.startY      = y;
    touchDrag.startScroll = local->scrollPx;
    return;
    }

  if(!touchDrag.active)
    return;
  if(phase>=2) {
    touchDrag = TouchDrag();
    return;
    }

  const Page* list = touchDrag.list==1 ? pagePl.get() : pageOth.get();
  if(list==nullptr)
    return;

  // The list follows the finger, dragging up reveals the lower rows
  auto&       local = pageLocal[touchDrag.list];
  const float side  = float(std::max(1,slotSize().h));
  local.scrollPx = std::clamp(touchDrag.startScroll+(touchDrag.startY-y),0.f,touchMaxScroll(*list));
  local.scroll   = size_t(local.scrollPx/side);

  // The selected item stays on the screen, so the buttons never act on a hidden one
  const size_t first = local.scroll*TouchColumns;
  const size_t last  = (local.scroll+rowsCount())*TouchColumns;
  if(local.sel<first)
    local.sel = first; else
  if(local.sel>=last)
    local.sel = last-1;
  local.sel = std::min(local.sel,std::max<size_t>(list->size(),1)-1);
  update();
  }

void InventoryMenu::drawAllTouch(Painter& p, Npc& player, DrawPass pass) {
  if(state==State::LockPicking)
    return;

  const auto lay   = touchLayout();
  const int  wcount = int(columsCount);
  const int  hcount = int(rowsCount());

  auto list = [&](const Page& inv, const PageLocal& sel, size_t index) {
    const auto& r = lay.list[index];
    drawItems(p,pass,inv,sel,int(r.x),int(r.y),wcount,hcount);
    if(pass==DrawPass::Front)
      drawScrollHints(p,inv,sel,r);
    };

  if(chest!=nullptr) {
    if(pass==DrawPass::Back)
      drawTouchHeader(p,chest->displayName(),lay.header[0]);
    list(*pageOth,pageLocal[0],0);
    }

  if(trader!=nullptr) {
    if(pass==DrawPass::Back)
      drawTouchHeader(p,trader->displayName(),lay.header[0]);
    list(*pageOth,pageLocal[0],0);
    }

  if(state!=State::Ransack) {
    if(pass==DrawPass::Back)
      drawGold(p,player,0,0);
    list(*pagePl,pageLocal[1],1);
    }

  if(pass==DrawPass::Back)
    drawInfo(p);
  }

void InventoryMenu::drawTouchHeader(Painter& p, std::string_view title, const Mobile::Rect& r) {
  const int x = int(r.x), y = int(r.y), dw = int(r.w), dh = int(r.h);
  const int border = std::max(2,dh/16);

  p.setBrush(Brush(Color(0.50f,0.52f,0.55f,0.95f)));
  p.drawRect(x,y,dw,dh);
  p.setBrush(Brush(Color(0.03f,0.10f,0.19f,0.96f)));
  p.drawRect(x+border,y+border,dw-2*border,dh-2*border);

  auto& fnt = Resources::font(touchTextScale());
  const auto sz = fnt.textSize(title);
  fnt.drawText(p,x+(dw-sz.w)/2,y+dh/2+sz.h/2,title);
  }

/// Arrows over the lists, when there are items out of sight
void InventoryMenu::drawScrollHints(Painter& p, const Page& inv, const PageLocal& sel, const Mobile::Rect& r) {
  const float side = r.w/float(Mobile::InventoryLayout::Columns);
  const float a    = side*0.14f;
  const float cx   = r.x+r.w/2;

  p.setBrush(Brush(Color(1.f,1.f,1.f,0.75f)));
  if(sel.scrollPx<touchMaxScroll(inv)-1.f) {
    const float y = r.y+r.h-side*0.16f;
    p.drawTriangle(cx-a*1.4f,y-a*0.7f,0.f,0.f, cx+a*1.4f,y-a*0.7f,0.f,0.f, cx,y+a*0.7f,0.f,0.f);
    }
  if(sel.scrollPx>1.f) {
    const float y = r.y+side*0.16f;
    p.drawTriangle(cx-a*1.4f,y+a*0.7f,0.f,0.f, cx+a*1.4f,y+a*0.7f,0.f,0.f, cx,y-a*0.7f,0.f,0.f);
    }
  }

void InventoryMenu::drawInfoTouch(Painter& p) {
  auto& pg  = activePage();
  auto& sel = activePageSel();
  auto  it  = pg.get(sel.sel);
  if(!it.isValid())
    return;
  auto& item = *pg.get(sel.sel);

  const auto  lay = touchLayout();
  const auto& r   = lay.info;
  if(r.w<=0)
    return;

  const float H   = float(h());
  const int   x   = int(r.x);
  const int   y   = int(r.y);
  const int   dw  = int(r.w);
  const int   dh  = int(r.h);
  const int   in  = int(float(w())*0.0126f);

  p.setBrush(Brush(Color(0.01f,0.015f,0.02f,0.62f)));
  p.drawRect(x,y,dw,dh);

  auto& fnt = Resources::font(touchTextScale());
  const int pix = fnt.pixelSize();

  // title in the center
  auto  desc  = item.description();
  auto& title = Resources::font(touchTextScale()*1.05f);
  title.drawText(p,x+(dw-title.textSize(desc).w)/2,y+int(H*0.05f)+title.pixelSize()/2,desc);

  // The separator of the actions. Properties are packed above it.
  const int sepY = int(lay.use.y-H*0.022f);
  p.setBrush(Brush(Color(0.62f,0.58f,0.50f,0.85f)));
  p.drawRect(x,sepY,dw,std::max(2,int(H*0.002f)));

  int rows = 0;
  for(size_t i=0; i<Item::MAX_UI_ROWS; ++i) {
    if(!item.uiText(i).empty())
      ++rows;
    }
  const int rowH = pix+pix/3;
  int       ry   = sepY-int(H*0.016f)-(rows-1)*rowH;
  const int propsTop = ry-rowH;
  for(size_t i=0; i<Item::MAX_UI_ROWS; ++i) {
    auto    txt = item.uiText(i);
    int32_t val = item.uiValue(i);
    if(txt.empty())
      continue;
    if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg.is(&player->inventory()))
      val = item.sellCost();

    fnt.drawText(p,x+in,ry,txt);
    if(val!=0) {
      string_frm vint(val);
      fnt.drawText(p,x+dw-in-fnt.textSize(vint).w,ry,vint);
      }
    ry += rowH;
    }

  // The model fills the space between the title and the properties
  const int top = y+title.pixelSize()*2;
  const int sz  = std::min({propsTop-top,int(float(lay.info.h)*0.55f),dw/2});
  if(sz>int(H*0.05f))
    renderer.drawItem(x+(dw-sz)/2,top,sz,sz,item);
  }

std::string_view InventoryMenu::mobileActionLabel() {
  if(player==nullptr || state==State::Closed || state==State::LockPicking)
    return "";
  auto& pg  = activePage();
  auto& sel = activePageSel();
  auto  it  = pg.get(sel.sel);
  if(!it.isValid())
    return "";

  const bool own = pg.is(&player->inventory());
  switch(state) {
    case State::Chest:   return own ? "Store" : "Take";
    case State::Trade:   return own ? "Sell"  : "Buy";
    case State::Ransack: return "Take";
    default:             break;
    }

  if(it.isEquipped())
    return "Unequip";
  const auto flag = it->mainFlag();
  if(flag&(ITM_CAT_NF|ITM_CAT_FF|ITM_CAT_MUN|ITM_CAT_ARMOR|ITM_CAT_RUNE|ITM_CAT_LIGHT|ITM_CAT_MAGIC))
    return "Equip";
  if(flag&ITM_CAT_DOCS)
    return "Read";
  return "Use";
  }
#endif

void InventoryMenu::mobileScroll(float dy) {
  if(player==nullptr || state==State::Closed)
    return;
  if(dy<0)
    moveDown(); else
    moveUp();
  adjustScroll();
  update();
  }

bool InventoryMenu::hasSelectedItem() {
  return player!=nullptr && state!=State::Closed && activePageSel().sel<activePage().size();
  }

void InventoryMenu::setCategory(int category) {
  takeTimer.stop();
  takeCount = 0;

  const auto c = Category(std::clamp(category,0,int(CatCount)-1));
  if(pagePl!=nullptr)
    pagePl->category = c;
  if(pageOth!=nullptr)
    pageOth->category = c;
  pageLocal[0] = PageLocal();
  pageLocal[1] = PageLocal();
  adjustScroll();
  update();
  }

int InventoryMenu::category() const {
  if(pagePl!=nullptr)
    return pagePl->category;
  if(pageOth!=nullptr)
    return pageOth->category;
  return CatAll;
  }
