#include "inventorymenu.h"

#include <Tempest/Painter>

#include "world/world.h"
#include "resources.h"

using namespace Tempest;

InventoryMenu::InventoryMenu() {
  slot = Resources::loadTexture("INV_SLOT.TGA");
  selT = Resources::loadTexture("INV_SLOT_HIGHLIGHTED.TGA");
  selU = Resources::loadTexture("INV_SLOT_EQUIPPED.TGA");
  tex  = Resources::loadTexture("INV_BACK.TGA");
  // TRADE_VALUE_MULTIPLIER
  setFocusPolicy(NoFocus);
  takeTimer.timeout.bind(this,&InventoryMenu::onTakeStuff);
  }

void InventoryMenu::setWorld(const World *w) {
  world=w;
  update();
  }

void InventoryMenu::close() {
  if(player!=nullptr){
    player->setInteraction(nullptr);
    player = nullptr;
    chest  = nullptr;
    }

  page  = 0;
  state = State::Closed;
  owner()->setFocus(true);
  update();
  }

void InventoryMenu::open(Npc &pl) {
  state  = State::Equip;
  player = &pl;
  chest  = nullptr;
  page   = 0;
  adjustScroll();
  setFocus(true);
  update();
  }

void InventoryMenu::open(Npc &pl, Interactive &ch) {
  state  = State::Chest;
  player = &pl;
  chest  = &ch;
  page   = 0;
  pl.setInteraction(chest);
  adjustScroll();
  setFocus(true);
  update();
  }

InventoryMenu::State InventoryMenu::isOpen() const {
  return state;
  }

void InventoryMenu::keyDownEvent(KeyEvent &e) {
  if(state==State::Closed){
    e.ignore();
    return;
    }

  auto pg=activePage();
  if(pg==nullptr)
    return;

  const size_t pCount=pagesCount();

  if(e.key==KeyEvent::K_W){
    if(sel>=columsCount)
      sel -= columsCount;
    }
  else if(e.key==KeyEvent::K_S){
    if(sel+columsCount<pg->recordsCount())
      sel += columsCount;
    }
  else if(e.key==KeyEvent::K_A){
    if(sel%columsCount==0 && page>0)
      page--;
    else if(sel>0)
      sel--;
    }
  else if(e.key==KeyEvent::K_D) {
    if(((sel+1)%columsCount==0 || sel+1==pg->recordsCount() || pg->recordsCount()==0) && page+1<pCount) {
      page++;
      }
    else if(sel+1<pg->recordsCount())
      sel++;
    }
  adjustScroll();
  update();
  }

void InventoryMenu::keyUpEvent(KeyEvent &e) {
  if(e.key==KeyEvent::K_ESCAPE || e.key==KeyEvent::K_Tab){
    close();
    }
  }

void InventoryMenu::mouseDownEvent(MouseEvent &e) {
  if(player==nullptr || state==State::Closed || e.button!=Event::ButtonLeft) {
    e.ignore();
    return;
    }

  auto page=activePage();
  if(page==nullptr)
    return;

  if(sel>=page->recordsCount())
    return;
  auto& r = page->at(sel);
  if(state==State::Equip) {
    if(r.isEquiped())
      player->unequipItem(r.clsId()); else
      player->useItem    (r.clsId());
    }
  else if(state==State::Chest) {
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
  if(player==nullptr)
    return;

  if(e.delta>0){
    if(sel>=columsCount)
      sel -= columsCount;
    }
  else if(e.delta<0){
    if(sel+columsCount<player->inventory().recordsCount())
      sel += columsCount;
    }
  adjustScroll();
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
  if(state==State::Chest)
    return 2;
  return 1;
  }

const Inventory *InventoryMenu::activePage() {
  const Inventory* pl = player==nullptr ? nullptr : &player->inventory();

  if(chest!=nullptr){
    return page==0 ? &chest->inventory() : pl;
    }
  return pl;
  }

void InventoryMenu::onTakeStuff() {
  auto page=activePage();
  if(page==nullptr)
    return;
  if(sel>=page->recordsCount())
    return;
  auto& r = page->at(sel);

  if(state==State::Chest) {
    if(page==&player->inventory()){
      player->moveItem(r.clsId(),*chest);
      } else {
      player->addItem(r.clsId(),*chest);
      }
    }
  adjustScroll();
  }

void InventoryMenu::adjustScroll() {
  auto page=activePage();
  if(page==nullptr)
    return;
  sel = std::min(sel, std::max<size_t>(page->recordsCount(),1)-1);
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

  drawGold(p,player,w()-padd-2*slotSize().w,70);

  const int wcount = int(columsCount);
  const int hcount = int(rowsCount());

  if(chest!=nullptr){
    drawItems(p,chest->inventory(),padd,iy,wcount,hcount);
    }

  drawItems(p,player.inventory(),w()-padd-wcount*slotSize().w,iy,wcount,hcount);
  drawInfo(p);
  }

void InventoryMenu::drawItems(Painter &p,const Inventory &inv, int x,int y, int wcount, int hcount) {
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

void InventoryMenu::drawSlot(Painter &p,const Inventory &inv, int x, int y, size_t id) {
  if(!slot)
    return;
  p.setBrush(*slot);
  p.drawRect(x,y,slotSize().w,slotSize().h,
             0,0,slot->w(),slot->h());

  if(id>=inv.recordsCount())
    return;
  auto& r    = inv.at(id);
  auto  page = activePage();

  if(id==sel && &inv==page && selT!=nullptr){
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
    std::snprintf(vint,sizeof(vint),"%d",r.slot());
    auto sz = p.font().textSize(vint);
    p.drawText(x+slotSize().w-sz.w-10,
               y+int(p.font().pixelSize()),
               vint);
    }
  }

void InventoryMenu::drawGold(Painter &p, Npc &player, int x, int y) {
  if(!slot)
    return;
  auto*          txt  = world ? world->script()->currencyName() : "";
  const uint32_t gold = player.inventory().goldCount();
  char           vint[64]={};
  if(txt==nullptr)
    txt="Gold";

  std::snprintf(vint,sizeof(vint),"%s : %d",txt,gold);
  const int dw = slotSize().w*2;
  const int dh = 34;
  const int tw = p.font().textSize(vint).w;
  const int th = p.font().textSize(vint).h;

  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh, 0,0,tex->w(),tex->h());
    }
  if(slot) {
    p.setBrush(*slot);
    p.drawRect(x,y,dw,dh, 0,0,slot->w(),slot->h());
    }
  p.drawText(x+(dw-tw)/2,y+dh/2+th/2,vint);
  }

void InventoryMenu::drawInfo(Painter &p) {
  p.setFont(Resources::font());
  const int dw   = std::min(w(),720);
  const int dh   = infoHeight();//int(choise.size()*p.font().pixelSize())+2*padd;
  const int x    = (w()-dw)/2;
  const int y    = h()-dh-20;

  const Inventory* pg=activePage();
  if(pg==nullptr || sel>=pg->recordsCount())
    return;

  auto& r = pg->at(sel);
  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh,
               0,0,tex->w(),tex->h());
    }

  int tw = p.font().textSize(r.description()).w;
  p.drawText(x+(dw-tw)/2,y+int(p.font().pixelSize()),r.description());

  for(size_t i=0;i<Item::MAX_UI_ROWS;++i){
    const char*   txt=r.uiText(i);
    const int32_t val=r.uiValue(i);
    char          vint[32]={};

    if(txt==nullptr || txt[0]=='\0')
      continue;

    std::snprintf(vint,sizeof(vint),"%d",val);
    int tw = p.font().textSize(vint).w;

    p.drawText(x+20,      y+int((i+2)*p.font().pixelSize()),txt);
    if(val!=0)
      p.drawText(x+dw-tw-20,y+int((i+2)*p.font().pixelSize()),vint);
    }
  }

