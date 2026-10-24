#include "inventorymenu.h"

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

  size_t                      size() const {
    if(is(nullptr))
      return 0;
    size_t ret = 0;
    auto it = iterator();
    while(it.isValid()) {
      ret++;
      ++it;
      }
    return ret;
    }
  Inventory::Iterator         get(size_t id) const {
    auto it = iterator();
    for(size_t i=0; i<id && it.isValid(); ++i)
      ++it;
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

  setFocusPolicy(NoFocus);
  setCursorShape(CursorShape::Hidden);
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
  resetOffers();
  barter = false;
  choice = false;
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
  resetOffers();
  state  = State::Trade;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  barter = Gothic::inst().version().game==1;
  choice = false;
  pagePl .reset(new InvPage  (pl.inventory()));
  pageOth.reset(new TradePage(tr.inventory()));
  if(barter) {
    pageOfferOth.reset(new InvPage(offerOth));
    pageOfferPl .reset(new InvPage(offerPl));
    }
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
    pageOfferOth.reset();
    pageOfferPl .reset();
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

  if(usePage && sel.sel%colsCount()==0 && page>0)
    page--;
  else if(sel.sel>0)
    sel.sel--;
  }

void InventoryMenu::moveRight(bool usePage) {
  auto&        pg     = activePage();
  auto&        sel    = activePageSel();
  const size_t pCount = pagesCount();

  if(usePage && ((sel.sel+1u)%colsCount()==0 || sel.sel+1u==pg.size() || pg.size()==0) && page+1u<pCount)
    page++;
  else if(sel.sel+1<pg.size())
    sel.sel++;
  }

void InventoryMenu::moveUp() {
  auto& sel = activePageSel();

  if(sel.sel>=colsCount())
    sel.sel -= colsCount();
  else
    moveLeft(false);
  }

void InventoryMenu::moveDown() {
  auto& pg  = activePage();
  auto& sel = activePageSel();

  if(sel.sel+colsCount()<pg.size())
    sel.sel += colsCount();
  else
    moveRight(false);
  }

void InventoryMenu::keyDownEvent(KeyEvent &e) {
  if(state==State::Closed || state==State::LockPicking){
    e.ignore();
    return;
    }

  if(choice) {
    auto key = keycodec.tr(e);
    if(key==KeyCodec::Forward && choiceSel>0)
      choiceSel--;
    else if(key==KeyCodec::Back && choiceSel<2)
      choiceSel++;
    else if(key==KeyCodec::ActionGeneric || e.key==KeyEvent::K_Return)
      onChoice();
    else if(e.key==KeyEvent::K_ESCAPE || key==KeyCodec::Inventory)
      choice = false;
    update();
    return;
    }

  if(barter && e.key==KeyEvent::K_Return) {
    choice    = true;
    choiceSel = 0;
    update();
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
  if(state==State::LockPicking || state==State::Closed || choice)
    return;
  processMove(e);
  adjustScroll();
  update();
  }

void InventoryMenu::keyUpEvent(KeyEvent&) {
  takeTimer.stop();
  lootMode = LootMode::Normal;
  }

void InventoryMenu::mouseDownEvent(MouseEvent &e) {
  if(player==nullptr || state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  if(choice) {
    if(e.button==MouseEvent::ButtonLeft)
      onChoice(); else
      choice = false;
    update();
    return;
    }

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

  if(state==State::LockPicking || choice)
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
  int iy=30+34+70;
  return size_t((h()-iy-infoHeight()-20)/slotSize().h);
  }

size_t InventoryMenu::colsCount() const {
  if(!barter)
    return columsCount;
  if(page==1 || page==2)
    return 1; // offers
  return barterColsCount();
  }

size_t InventoryMenu::barterColsCount() const {
  // trader and player panels, two one-column offers in the middle
  const int padd = 43;
  const int sw   = slotSize().w;
  const int cnt  = (w()-2*padd-2*sw-2*(sw/4))/(2*sw);
  return std::min(size_t(std::max(cnt,1)),columsCount);
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
  const float scale = Gothic::interfaceScale(this);
  const float cell  = float(Gothic::options().inventoryCellSize);
  return Size(int(cell*scale),int(cell*scale));
  }

int InventoryMenu::infoHeight() const {
  const float scale = Gothic::interfaceScale(this);
  return (Item::MAX_UI_ROWS+2)*int(float(Resources::font(scale).pixelSize()))+int(scale*10)/*padding bottom*/;
  }

size_t InventoryMenu::pagesCount() const {
  if(barter)
    return 4;
  if(state==State::Chest || state==State::Trade)
    return 2;
  return 1;
  }

const InventoryMenu::Page &InventoryMenu::activePage() {
  if(barter) {
    switch(page) {
      case 0:  return *pageOth;
      case 1:  return *pageOfferOth;
      case 2:  return *pageOfferPl;
      default: return *pagePl;
      }
    }
  if(pageOth!=nullptr)
    return *(page==0 ? pageOth : pagePl);
  if(pagePl)
    return *pagePl;

  static Page n;
  return n;
  }

InventoryMenu::PageLocal &InventoryMenu::activePageSel() {
  if(barter) {
    static const uint8_t id[] = {0,2,3,1};
    return pageLocal[id[page]];
    }
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
  else if(state==State::Trade && barter) {
    const size_t cls = it->clsId();
    if(page.is(&trader->inventory()))
      trader->moveItem(cls,offerOth,itemCount);
    else if(page.is(&offerOth))
      trader->addItem(cls,offerOth,itemCount);
    else if(page.is(&offerPl))
      player->addItem(cls,offerPl,itemCount);
    else
      player->moveItem(cls,offerPl,itemCount);
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

void InventoryMenu::onChoice() {
  choice = false;
  if(choiceSel==0) {
    // accept, if player offers at least the same value
    if(offerValue(offerPl)<offerValue(offerOth))
      return;
    for(auto it=offerOth.iterator(Inventory::T_Inventory); it.isValid(); it=offerOth.iterator(Inventory::T_Inventory))
      player->addItem(it->clsId(),offerOth,it->count());
    for(auto it=offerPl.iterator(Inventory::T_Inventory); it.isValid(); it=offerPl.iterator(Inventory::T_Inventory))
      trader->addItem(it->clsId(),offerPl,it->count());
    close();
    }
  else if(choiceSel==1) {
    resetOffers();
    }
  else {
    close();
    }
  }

void InventoryMenu::resetOffers() {
  if(!barter || player==nullptr || trader==nullptr)
    return;
  for(auto it=offerOth.iterator(Inventory::T_Inventory); it.isValid(); it=offerOth.iterator(Inventory::T_Inventory))
    trader->addItem(it->clsId(),offerOth,it->count());
  for(auto it=offerPl.iterator(Inventory::T_Inventory); it.isValid(); it=offerPl.iterator(Inventory::T_Inventory))
    player->addItem(it->clsId(),offerPl,it->count());
  }

int64_t InventoryMenu::offerValue(const Inventory& inv) {
  int64_t ret = 0;
  for(auto it=inv.iterator(Inventory::T_Inventory); it.isValid(); ++it)
    ret += int64_t(it->cost())*int64_t(it.count());
  return ret;
  }

void InventoryMenu::adjustScroll() {
  auto& page = activePage();
  auto& sel  = activePageSel();
  sel.sel = std::min(sel.sel, std::max<size_t>(page.size(),1)-1);
  while(sel.sel<sel.scroll*colsCount()) {
    if(sel.scroll<=1){
      sel.scroll=0;
      return;
      }
    sel.scroll-=1;
    }

  const size_t hcount=rowsCount();
  while(sel.sel>=(sel.scroll+hcount)*colsCount()) {
    sel.scroll+=1;
    }
  }

void InventoryMenu::drawAll(Painter &p, Npc &player, DrawPass pass) {
  const int padd = 43;

  int iy=30+34+70;

  if(state==State::LockPicking)
    return;

  const int wcount = int(colsCount());
  const int hcount = int(rowsCount());

  if(barter) {
    const int sw = slotSize().w;
    const int wc = int(barterColsCount());
    // like vanilla: trader on the left, player on the right, offers next to each other in the middle
    const int x3 = w()-padd-wc*sw;
    const int x1 = w()/2-sw;
    const int x2 = w()/2;
    if(pass==DrawPass::Back) {
      drawHeader(p,trader->displayName(),padd,70);
      drawHeader(p,string_frm(int(offerValue(offerOth))),x1,70,sw);
      drawHeader(p,string_frm(int(offerValue(offerPl))), x2,70,sw);
      drawGold  (p,player,w()-padd-2*sw,70);
      }
    drawItems(p,pass,*pageOth,     pageLocal[0],padd,iy,wc,hcount);
    drawItems(p,pass,*pageOfferOth,pageLocal[2],x1,  iy,1, hcount);
    drawItems(p,pass,*pageOfferPl, pageLocal[3],x2,  iy,1, hcount);
    drawItems(p,pass,*pagePl,      pageLocal[1],x3,  iy,wc,hcount);
    if(pass==DrawPass::Back) {
      if(choice)
        drawChoice(p); else
        drawInfo(p);
      }
    return;
    }

  if(chest!=nullptr){
    if(pass==DrawPass::Back)
      drawHeader(p,chest->displayName(),padd,70);
    drawItems(p,pass,*pageOth,pageLocal[0],padd,iy,wcount,hcount);
    }

  if(trader!=nullptr) {
    if(pass==DrawPass::Back)
      drawHeader(p,trader->displayName(),padd,70);
    drawItems(p,pass,*pageOth,pageLocal[0],padd,iy,wcount,hcount);
    }

  if(state!=State::Ransack) {
    if(pass==DrawPass::Back)
      drawGold (p,player,w()-padd-2*slotSize().w,70);
    drawItems(p,pass,*pagePl,pageLocal[1],w()-padd-wcount*slotSize().w,iy,wcount,hcount);
    }

  if(pass==DrawPass::Back)
    drawInfo(p);
  }

void InventoryMenu::drawItems(Painter &p, DrawPass pass,
                              const Page &inv, const PageLocal& sel, int x0, int y, int wcount, int hcount) {
  if(state==State::LockPicking)
    return;

  if(tex!=nullptr && pass==DrawPass::Back) {
    p.setBrush(*tex);
    p.drawRect(x0,y,slotSize().w*wcount,slotSize().h*hcount,
               0,0,tex->w(),tex->h());
    }

  auto   it = inv.iterator();
  size_t id = 0;
  for(size_t i=0; it.isValid() && i<sel.scroll*size_t(wcount); ++i) {
    ++it;
    ++id;
    }
  for(int i=0;i<hcount;++i) {
    for(int r=0;r<wcount;++r) {
      const int x = x0 + r*slotSize().w;
      if(pass==DrawPass::Back) {
        p.setBrush(*slot);
        p.drawRect(x,y,slotSize().w,slotSize().h,
                   0,0,slot->w(),slot->h());
        }
      if(it.isValid()) {
        drawSlot(p,pass, it,inv,sel, x,y, id);
        ++it;
        ++id;
        }
      }
    y += slotSize().h;
    }
  }

void InventoryMenu::drawSlot(Painter &p, DrawPass pass, const Inventory::Iterator &it,
                             const Page& page, const PageLocal &sel,
                             int x, int y, size_t id) {
  if(!slot)
    return;

  auto& active = activePage();
  const float scale = Gothic::interfaceScale(this);

  if(pass==DrawPass::Back) {
    if((!it.isValid() && id==0) || (id==sel.sel && &page==&active)){
      p.setBrush(*selT);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selT->w(),selT->h());
      }

    if(it.isEquipped() && selU!=nullptr) {
      p.setBrush(*selU);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selU->w(),selU->h());
      }

    const int dsz = (id==sel.sel ? 5 : 0);
    renderer.drawItem(x-dsz, y-dsz, slotSize().w+2*dsz, slotSize().h+2*dsz, *it);
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
  drawHeader(p,vint,x,y);
  }

void InventoryMenu::drawHeader(Painter &p, std::string_view title, int x, int y, int minW) {
  const float scale = Gothic::interfaceScale(this);
  auto&       fnt   = Resources::font(scale);

  const int   tw    = fnt.textSize(title).w;
  const int   th    = fnt.textSize(title).h;
  const int   padd  = int(8*scale);
  const int   dw    = std::max(minW>0 ? minW : slotSize().w*2, tw+padd*2);
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
  const float scale = Gothic::interfaceScale(this);
  const int   dw    = std::min(w(), int(720*scale));
  const int   dh    = infoHeight();
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
  }

void InventoryMenu::drawChoice(Painter& p) {
  const float scale = Gothic::interfaceScale(this);
  const int   dw    = std::min(w(), int(720*scale));
  const int   dh    = infoHeight();
  const int   x     = (w()-dw)/2;
  const int   y     = h()-dh-20;

  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh,
               0,0,tex->w(),tex->h());
    }

  static const char* name[] = {"STR_INFO_TRADE_ACCEPT", "STR_INFO_TRADE_RESET", "STR_INFO_TRADE_EXIT"};
  static const char* def [] = {"Accept", "Reset", "Exit"};
  auto world = Gothic::inst().world();
  for(uint8_t i=0; i<3; ++i) {
    std::string_view txt = def[i];
    if(auto sym = world!=nullptr ? world->script().findSymbol(name[i]) : nullptr)
      txt = sym->get_string();
    auto& fnt = Resources::font(i==choiceSel ? Resources::FontType::Hi : Resources::FontType::Normal, scale);
    int   tw  = fnt.textSize(txt).w;
    fnt.drawText(p,x+(dw-tw)/2,y+int(i+2)*int(fnt.pixelSize()),txt);
    }
  }

void InventoryMenu::draw(Tempest::Encoder<CommandBuffer>& cmd) {
  renderer.draw(cmd);
  }
