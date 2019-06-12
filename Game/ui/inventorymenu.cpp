#include "inventorymenu.h"

#include <Tempest/Painter>
#include <Tempest/SoundEffect>

#include "utils/cp1251.h"
#include "world/world.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

struct InventoryMenu::InvPage : InventoryMenu::Page {
  InvPage(const Inventory& i):inv(i){}

  bool   is(const Inventory* i) const override { return &inv==i; }
  size_t size() const override { return inv.recordsCount(); }
  const Item& operator[](size_t i) const override { return inv.at(i); }

  const Inventory& inv;
  };

struct InventoryMenu::TradePage : InventoryMenu::Page {
  TradePage(const Inventory& i):inv(i){}

  bool   is(const Inventory* i) const override { return &inv==i; }
  size_t size() const override { return inv.tradableCount(); }
  const Item& operator[](size_t i) const override { return inv.atTrade(i); }

  const Inventory& inv;
  };

struct InventoryMenu::RansackPage : InventoryMenu::Page {
  RansackPage(const Inventory& i):inv(i){}

  bool   is(const Inventory* i) const override { return &inv==i; }
  size_t size() const override { return inv.ransackCount(); }
  const Item& operator[](size_t i) const override { return inv.atRansack(i); }

  const Inventory& inv;
  };

InventoryMenu::InventoryMenu(Gothic &gothic, const RendererStorage &storage)
  :gothic(gothic),renderer(storage) {
  slot = Resources::loadTexture("INV_SLOT.TGA");
  selT = Resources::loadTexture("INV_SLOT_HIGHLIGHTED.TGA");
  selU = Resources::loadTexture("INV_SLOT_EQUIPPED.TGA");
  tex  = Resources::loadTexture("INV_BACK.TGA");

  setFocusPolicy(NoFocus);
  takeTimer.timeout.bind(this,&InventoryMenu::onTakeStuff);
  }

void InventoryMenu::close() {
  if(player!=nullptr){
    player->setInteraction(nullptr);
    player = nullptr;
    chest  = nullptr;
    }

  if(state!=State::Closed) {
    if(state==State::Trade)
      gothic.emitGlobalSound("TRADE_CLOSE"); else
      gothic.emitGlobalSound("INV_CLOSE");
    }

  page  = 0;
  pagePl .reset();
  pageOth.reset();
  state = State::Closed;
  update();
  }

void InventoryMenu::open(Npc &pl) {
  state  = State::Equip;
  player = &pl;
  trader = nullptr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage  (pl.inventory()));
  pageOth.reset();
  adjustScroll();
  update();

  gothic.emitGlobalSound("INV_OPEN");
  //gothic.emitGlobalSound("INV_CHANGE");
  }

void InventoryMenu::trade(Npc &pl, Npc &tr) {
  state  = State::Trade;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage  (pl.inventory()));
  pageOth.reset(new TradePage(tr.inventory()));
  adjustScroll();
  update();
  gothic.emitGlobalSound("TRADE_OPEN");
  }

bool InventoryMenu::ransack(Npc &pl, Npc &tr) {
  if(tr.inventory().ransackCount()==0)
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
  gothic.emitGlobalSound("INV_OPEN");
  return true;
  }

void InventoryMenu::open(Npc &pl, Interactive &ch) {
  state  = State::Chest;
  player = &pl;
  trader = nullptr;
  chest  = &ch;
  page   = 0;
  pagePl .reset(new InvPage(pl.inventory()));
  pageOth.reset(new InvPage(ch.inventory()));
  pl.setInteraction(chest);
  adjustScroll();
  update();
  }

InventoryMenu::State InventoryMenu::isOpen() const {
  return state;
  }

bool InventoryMenu::isActive() const {
  return state!=State::Closed;
  }

void InventoryMenu::tick(uint64_t /*dt*/) {
  if(state==State::Ransack){
    if(trader==nullptr){
      close();
      return;
      }

    if(!trader->isDown())
      close();
    if(trader->inventory().ransackCount()==0)
      close();
    }
  }

void InventoryMenu::keyDownEvent(KeyEvent &e) {
  if(state==State::Closed){
    e.ignore();
    return;
    }

  auto&        pg     = activePage();
  const size_t pCount = pagesCount();

  if(e.key==KeyEvent::K_W){
    if(sel>=columsCount)
      sel -= columsCount;
    }
  else if(e.key==KeyEvent::K_S){
    if(sel+columsCount<pg.size())
      sel += columsCount;
    }
  else if(e.key==KeyEvent::K_A){
    if(sel%columsCount==0 && page>0){
      page--;
      sel += (columsCount-1);
      }
    else if(sel>0)
      sel--;
    }
  else if(e.key==KeyEvent::K_D) {
    if(((sel+1)%columsCount==0 || sel+1==pg.size() || pg.size()==0) && page+1<pCount) {
      page++;
      sel -= sel%columsCount;
      }
    else if(sel+1<pg.size())
      sel++;
    }
  adjustScroll();
  update();
  }

void InventoryMenu::keyUpEvent(KeyEvent &e) {
  if(e.key==KeyEvent::K_ESCAPE || (e.key==KeyEvent::K_Tab && state!=State::Trade)){
    close();
    }
  }

void InventoryMenu::mouseDownEvent(MouseEvent &e) {
  if(player==nullptr || state==State::Closed || e.button!=Event::ButtonLeft) {
    e.ignore();
    return;
    }

  auto& page=activePage();
  if(sel>=page.size())
    return;
  auto& r = page[sel];
  if(state==State::Equip) {
    if(r.isEquiped())
      player->unequipItem(r.clsId()); else
      player->useItem    (r.clsId());
    }
  else if(state==State::Chest || state==State::Trade || state==State::Ransack) {
    takeTimer.start(100);
    onTakeStuff();
    }
  adjustScroll();
  }

void InventoryMenu::mouseUpEvent(MouseEvent&) {
  takeTimer.stop();
  }

void InventoryMenu::mouseWheelEvent(MouseEvent &e) {
  if(state==State::Closed) {
    e.ignore();
    return;
    }

  auto& pg=activePage();
  if(e.delta>0){
    if(sel>=columsCount)
      sel -= columsCount;
    }
  else if(e.delta<0){
    if(sel+columsCount<pg.size())
      sel += columsCount;
    }
  adjustScroll();
  }

const World *InventoryMenu::world() const {
  return gothic.world();
  }

size_t InventoryMenu::rowsCount() const {
  int iy=30+34+70;
  return size_t((h()-iy-infoHeight()-20)/slotSize().h);
  }

void InventoryMenu::paintEvent(PaintEvent &e) {
  if(player==nullptr || state==State::Closed)
    return;

  Painter p(e);
  drawAll(p,*player);
  }

Size InventoryMenu::slotSize() const {
  return Size(70,70);
  }

int InventoryMenu::infoHeight() const {
  return (Item::MAX_UI_ROWS+2)*int(Resources::font().pixelSize())+10/*padding bottom*/;
  }

size_t InventoryMenu::pagesCount() const {
  if(state==State::Chest || state==State::Trade)
    return 2;
  return 1;
  }

const InventoryMenu::Page &InventoryMenu::activePage() {
  if(pageOth!=nullptr){
    return *(page==0 ? pageOth : pagePl);
    }
  if(pagePl)
    return *pagePl;

  static Page n;
  return n;
  }

void InventoryMenu::onTakeStuff() {
  auto& page=activePage();
  if(sel>=page.size())
    return;
  auto& r = page[sel];

  if(state==State::Chest) {
    if(page.is(&player->inventory())){
      player->moveItem(r.clsId(),*chest);
      } else {
      player->addItem(r.clsId(),*chest);
      }
    }
  else if(state==State::Trade) {
    if(page.is(&player->inventory())){
      player->sellItem(r.clsId(),*trader);
      } else {
      player->buyItem(r.clsId(),*trader);
      }
    }
  else if(state==State::Ransack) {
    if(page.is(&trader->inventory())){
      player->addItem(r.clsId(),*trader);
      }
    }
  adjustScroll();
  }

void InventoryMenu::adjustScroll() {
  auto& page=activePage();
  sel = std::min(sel, std::max<size_t>(page.size(),1)-1);
  while(sel<scroll*columsCount) {
    if(scroll<=1){
      scroll=0;
      return;
      }
    scroll-=1;
    }

  const size_t hcount=rowsCount();
  while(sel>=(scroll+hcount)*columsCount) {
    scroll+=1;
    }
  }

void InventoryMenu::drawAll(Painter &p,Npc &player) {
  p.setFont(Resources::font());
  const int padd = 43;

  int iy=30+34+70;

  const int wcount = int(columsCount);
  const int hcount = int(rowsCount());

  if(chest!=nullptr){
    drawHeader(p,cp1251::toUtf8(chest->displayName()),padd,70);
    drawItems(p,*pageOth,padd,iy,wcount,hcount);
    }

  if(trader!=nullptr) {
    drawHeader(p,cp1251::toUtf8(trader->displayName()),padd,70);
    drawItems(p,*pageOth,padd,iy,wcount,hcount);
    }

  if(state!=State::Ransack) {
    drawGold (p,player,w()-padd-2*slotSize().w,70);
    drawItems(p,*pagePl,w()-padd-wcount*slotSize().w,iy,wcount,hcount);
    }
  drawInfo(p);
  }

void InventoryMenu::drawItems(Painter &p, const Page &inv, int x, int y, int wcount, int hcount) {
  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,slotSize().w*wcount,slotSize().h*hcount,
               0,0,tex->w(),tex->h());
    }

  for(int i=0;i<hcount;++i){
    for(int r=0;r<wcount;++r){
      int sx = x + r*slotSize().w;
      drawSlot(p,inv, sx,y, size_t((int(scroll)+i)*wcount+r));
      }
    y+=slotSize().h;
    }
  }

void InventoryMenu::drawSlot(Painter &p,const Page &inv, int x, int y, size_t id) {
  if(!slot)
    return;
  p.setBrush(*slot);
  p.drawRect(x,y,slotSize().w,slotSize().h,
             0,0,slot->w(),slot->h());

  if(id>=inv.size())
    return;
  auto& r    = inv[id];
  auto& page = activePage();

  if(id==sel && &page==&inv && selT!=nullptr){
    p.setBrush(*selT);
    p.drawRect(x,y,slotSize().w,slotSize().h,
               0,0,selT->w(),selT->h());
    }

  if(r.isEquiped() && selU!=nullptr){
    p.setBrush(*selU);
    p.drawRect(x,y,slotSize().w,slotSize().h,
               0,0,selU->w(),selU->h());
    }

  char  vint[32]={};
  std::snprintf(vint,sizeof(vint),"%d",r.count());
  auto sz = p.font().textSize(vint);
  p.drawText(x+slotSize().w-sz.w-10,
             y+slotSize().h-10,
             vint);

  if(r.slot()!=Item::NSLOT){
    p.setBrush(Color(1,0,0,1));
    std::snprintf(vint,sizeof(vint),"%d",r.slot());
    auto sz = p.font().textSize(vint);
    p.drawText(x+10,
               y+slotSize().h/2+sz.h/2,
               vint);
    p.setBrush(Color(1,1,1,1));
    }

  renderer.drawItem(x,y,slotSize().w,slotSize().h,r);
  }

void InventoryMenu::drawGold(Painter &p, Npc &player, int x, int y) {
  if(!slot)
    return;
  auto           w    = world();
  auto*          txt  = w ? w->script().currencyName() : nullptr;
  const uint32_t gold = player.inventory().goldCount();
  char           vint[64]={};
  if(txt==nullptr)
    txt="Gold";

  std::snprintf(vint,sizeof(vint),"%s : %d",txt,gold);
  drawHeader(p,vint,x,y);
  }

void InventoryMenu::drawHeader(Painter &p,const char* title, int x, int y) {
  const int dw = slotSize().w*2;
  const int dh = 34;
  const int tw = p.font().textSize(title).w;
  const int th = p.font().textSize(title).h;

  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh, 0,0,tex->w(),tex->h());
    }
  if(slot) {
    p.setBrush(*slot);
    p.drawRect(x,y,dw,dh, 0,0,slot->w(),slot->h());
    }
  p.drawText(x+(dw-tw)/2,y+dh/2+th/2,title);
  }

void InventoryMenu::drawInfo(Painter &p) {
  p.setFont(Resources::font());
  const int dw   = std::min(w(),720);
  const int dh   = infoHeight();//int(choise.size()*p.font().pixelSize())+2*padd;
  const int x    = (w()-dw)/2;
  const int y    = h()-dh-20;

  auto& pg=activePage();
  if(sel>=pg.size())
    return;

  auto& r = pg[sel];
  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh,
               0,0,tex->w(),tex->h());
    }

  auto desc = cp1251::toUtf8(r.description());
  int  tw   = p.font().textSize(desc).w;
  p.drawText(x+(dw-tw)/2,y+int(p.font().pixelSize()),desc);

  for(size_t i=0;i<Item::MAX_UI_ROWS;++i){
    const char*   txt=cp1251::toUtf8(r.uiText(i));
    int32_t       val=r.uiValue(i);
    char          vint[32]={};

    if(txt==nullptr || txt[0]=='\0')
      continue;

    if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg.is(&player->inventory())){
      val = r.sellCost();
      }

    std::snprintf(vint,sizeof(vint),"%d",val);
    int tw = p.font().textSize(vint).w;

    p.drawText(x+20,      y+int((i+2)*p.font().pixelSize()),txt);
    if(val!=0)
      p.drawText(x+dw-tw-20,y+int((i+2)*p.font().pixelSize()),vint);
    }
  }

void InventoryMenu::draw(CommandBuffer &cmd, uint32_t imgId) {
  renderer.draw(cmd,imgId);
  }
