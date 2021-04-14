#include "inventorymenu.h"

#include <Tempest/Painter>
#include <Tempest/SoundEffect>

#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "utils/gthfont.h"
#include "utils/keycodec.h"
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

InventoryMenu::InventoryMenu(Gothic &gothic, const KeyCodec& key, const RendererStorage &storage)
  :gothic(gothic), keycodec(key), renderer(storage) {
  slot = Resources::loadTexture("INV_SLOT.TGA");
  selT = Resources::loadTexture("INV_SLOT_HIGHLIGHTED.TGA");
  selU = Resources::loadTexture("INV_SLOT_EQUIPPED.TGA");
  tex  = Resources::loadTexture("INV_BACK.TGA"); // INV_TITEL.TGA

  int invMaxColumns = gothic.settingsGetI("GAME","invMaxColumns");
  if(invMaxColumns>0)
    columsCount = size_t(invMaxColumns); else
    columsCount = 5;

  setFocusPolicy(NoFocus);
  takeTimer.timeout.bind(this,&InventoryMenu::onTakeStuff);
  }

void InventoryMenu::close() {
  if(state!=State::Closed) {
    if(state==State::Trade)
      gothic.emitGlobalSound("TRADE_CLOSE"); else
      gothic.emitGlobalSound("INV_CLOSE");
    }
  renderer.reset();
  state  = State::Closed;
  }

void InventoryMenu::open(Npc &pl) {
  if(pl.isDown())
    return;
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
  gothic.emitGlobalSound("TRADE_OPEN");
  }

bool InventoryMenu::ransack(Npc &pl, Npc &tr) {
  if(tr.inventory().ransackCount()==0 || pl.isDown())
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
  if(pl.isDown())
    return;
  const bool needToPicklock = ch.needToLockpick(pl);
  if(!pl.setInteraction(&ch))
    return;

  if(needToPicklock && !ch.isCracked()) {
    pickLockProgress = 0;
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
  if(player!=nullptr && player->isDown()) {
    close();
    return;
    }

  if(state==State::Ransack) {
    if(trader==nullptr){
      close();
      return;
      }

    if(!trader->isDown())
      close();
    else if(trader->inventory().ransackCount()==0)
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
  auto&        pg     = activePage();
  auto&        sel    = activePageSel();
  const size_t pCount = pagesCount();

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

void InventoryMenu::processPickLock(KeyEvent& e) {
  auto&        script        = world()->script();
  const size_t ItKE_lockpick = script.getSymbolIndex("ItKE_lockpick");

  auto k  = keycodec.tr(e);
  char ch = '\0';
  if(k==KeyCodec::Left || k==KeyCodec::RotateL)
    ch = 'L';
  else if(k==KeyCodec::Right || k==KeyCodec::RotateR)
    ch = 'R';
  else if(k==KeyCodec::Back) {
    close();
    return;
    }
  else
    return;

  const std::string& cmp = chest->pickLockCode();
  if(pickLockProgress<cmp.size() && cmp[pickLockProgress]!=ch) {
    pickLockProgress = 0;
    const int32_t dex = player->attribute(Npc::ATR_DEXTERITY);
    if(dex<int32_t(script.rand(100)))  {
      script.invokePickLock(*player,0,1);
      player->delItem(ItKE_lockpick,1);
      if(player->inventory().itemCount(ItKE_lockpick)==0) {
        close();
        return;
        }
      } else {
      script.invokePickLock(*player,0,0);
      }
    } else {
    pickLockProgress++;
    if(pickLockProgress==cmp.size()) {
      script.invokePickLock(*player,1,1);
      chest->setAsCracked(true);
      pickLockProgress = 0;
      state            = State::Chest;
      } else {
      script.invokePickLock(*player,1,0);
      }
    }
  }

void InventoryMenu::moveLeft(bool usePage) {
  auto&        pg     = activePage();
  auto&        sel    = activePageSel();
  const size_t pCount = pagesCount();

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
  auto& pg  = activePage();
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
  if(state==State::Closed){
    e.ignore();
    return;
    }

  if(state==State::LockPicking) {
    processPickLock(e);
    return;
    }

  processMove(e);

  if(e.key==KeyEvent::K_Z) {
    lootMode = LootMode::Ten;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if(e.key==KeyEvent::K_X) {
    lootMode = LootMode::Hundred;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if(e.key==KeyEvent::K_Space) {
    lootMode = LootMode::Stack;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if(keycodec.tr(e)==KeyCodec::Jump) {
    lootMode = LootMode::Normal;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if (keycodec.tr(e)==KeyCodec::ActionGeneric) {
    onItemAction();
    }
  else if(e.key==KeyEvent::K_ESCAPE || keycodec.tr(e)==KeyCodec::Inventory){
    close();
    }

  adjustScroll();
  update();
  }

void InventoryMenu::keyRepeatEvent(KeyEvent& e) {
  if(state==State::LockPicking)
    return;
  processMove(e);
  adjustScroll();
  update();
  }

void InventoryMenu::keyUpEvent(KeyEvent &e) {
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

  if (e.button==MouseEvent::ButtonLeft)
    onItemAction();
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

  auto& pg  = activePage();
  auto& sel = activePageSel();
  if(e.delta>0)
    for(int i=0;i<e.delta/120;++i){
      moveUp();
      }
  else if(e.delta<0)
    for(int i=0;i<-e.delta/120;++i){
      moveDown();
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
  
void InventoryMenu::onItemAction() { 
  auto& page = activePage();
  auto& sel  = activePageSel();
  if(sel.sel>=page.size())
    return;

  auto& r = page[sel.sel];
  if(state==State::Equip) {
    if(r.isEquiped())
      player->unequipItem(r.clsId()); else
      player->useItem    (r.clsId());
    }
  else if(state==State::Chest || state==State::Trade || state==State::Ransack) {
    lootMode = LootMode::Normal;
    takeTimer.start(200);
    onTakeStuff();
    }
  }

void InventoryMenu::onTakeStuff() { 
  uint32_t itemCount = 0;
  auto& page = activePage();
  auto& sel = activePageSel();
  if(sel.sel >= page.size())
    return;
  auto& r = page[sel.sel];
  if(lootMode==LootMode::Normal) {
    ++takeCount;
    itemCount = uint32_t(std::pow(10,takeCount / 10));
    if(r.count() <= itemCount) {
      itemCount = uint32_t(r.count());
      takeCount = 0;
      }
    }
  else if(lootMode==LootMode::Stack) {
    itemCount = uint32_t(r.count());
    }
  else if(lootMode==LootMode::Ten) {
    itemCount = 10;
    }
  else if(lootMode==LootMode::Hundred) {
    itemCount = 100;
    }

  if(r.count() < itemCount) {
    itemCount = uint32_t(r.count());
    }

  if(state==State::Chest) {
    if(page.is(&player->inventory())) {
      player->moveItem(r.clsId(),*chest,itemCount);
      } else {
      player->addItem(r.clsId(),*chest,itemCount);
      }
    }
  else if(state==State::Trade) {
    if(page.is(&player->inventory())) {
      player->sellItem(r.clsId(),*trader,itemCount);
      } else {
      player->buyItem(r.clsId(),*trader,itemCount);
      }
    }
  else if(state==State::Ransack) {
    if(page.is(&trader->inventory())) {
      player->addItem(r.clsId(),*trader,itemCount);
      }
    }
  else if(state==State::Equip) {
    player->dropItem(r.clsId());
    }
  adjustScroll();
  }

void InventoryMenu::adjustScroll() {
  auto& page=activePage();
  auto& sel =activePageSel();
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

void InventoryMenu::drawAll(Painter &p,Npc &player,DrawPass pass) {
  const int padd = 43;

  int iy=30+34+70;

  if(state==State::LockPicking)
    return;

  const int wcount = int(columsCount);
  const int hcount = int(rowsCount());

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
                              const Page &inv, const PageLocal& sel, int x, int y, int wcount, int hcount) {
  if(state==State::LockPicking)
    return;

  if(tex && pass==DrawPass::Back) {
    p.setBrush(*tex);
    p.drawRect(x,y,slotSize().w*wcount,slotSize().h*hcount,
               0,0,tex->w(),tex->h());
    }

  for(int i=0;i<hcount;++i){
    for(int r=0;r<wcount;++r){
      int sx = x + r*slotSize().w;
      drawSlot(p,pass, inv,sel, sx,y, size_t((int(sel.scroll)+i)*wcount+r));
      }
    y+=slotSize().h;
    }
  }

void InventoryMenu::drawSlot(Painter &p, DrawPass pass, const Page &inv, const PageLocal &sel, int x, int y, size_t id) {
  if(!slot)
    return;

  auto& page = activePage();

  if(pass==DrawPass::Back){
    p.setBrush(*slot);
    p.drawRect(x,y,slotSize().w,slotSize().h,
               0,0,slot->w(),slot->h());

    if(inv.size()==0 && id==0 && &page==&inv){
      p.setBrush(*selT);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selT->w(),selT->h());
      }
    }

  if(id>=inv.size())
    return;

  auto& r = inv[id];

  if(pass==DrawPass::Back) {
    if(id==sel.sel && &page==&inv && selT!=nullptr){
      p.setBrush(*selT);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selT->w(),selT->h());
      }

    if(r.isEquiped() && selU!=nullptr){
      p.setBrush(*selU);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selU->w(),selU->h());
      }
    renderer.drawItem(x,y,slotSize().w,slotSize().h,r);
    } else {
    auto& fnt = Resources::font();
    char  vint[32]={};

    if(r.count()>1) {
      std::snprintf(vint,sizeof(vint),"%d",int(r.count()));
      auto sz = fnt.textSize(vint);
      fnt.drawText(p,x+slotSize().w-sz.w-10,
                   y+slotSize().h-10,
                   vint);
      }

    if(r.slot()!=Item::NSLOT) {
      auto& fnt = Resources::font(Resources::FontType::Red);
      std::snprintf(vint,sizeof(vint),"%d",int(r.slot()));
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
  auto           w    = world();
  auto*          txt  = w ? w->script().currencyName() : nullptr;
  const size_t   gold = player.inventory().goldCount();
  char           vint[64]={};
  if(txt==nullptr)
    txt="Gold";

  std::snprintf(vint,sizeof(vint),"%s : %u",txt,uint32_t(gold));
  drawHeader(p,vint,x,y);
  }

void InventoryMenu::drawHeader(Painter &p,const char* title, int x, int y) {
  auto& fnt = Resources::font();

  const int dw = slotSize().w*2;
  const int dh = 34;
  const int tw = fnt.textSize(title).w;
  const int th = fnt.textSize(title).h;

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
  const int dw   = std::min(w(),720);
  const int dh   = infoHeight();//int(choise.size()*p.font().pixelSize())+2*padd;
  const int x    = (w()-dw)/2;
  const int y    = h()-dh-20;

  auto& pg  = activePage();
  auto& sel = activePageSel();

  if(sel.sel>=pg.size())
    return;

  auto& r = pg[sel.sel];
  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh,
               0,0,tex->w(),tex->h());
    }

  auto& fnt = Resources::font();
  auto desc = r.description();
  int  tw   = fnt.textSize(desc).w;
  fnt.drawText(p,x+(dw-tw)/2,y+int(fnt.pixelSize()),desc);

  for(size_t i=0;i<Item::MAX_UI_ROWS;++i){
    const char*   txt=r.uiText(i);
    int32_t       val=r.uiValue(i);
    char          vint[32]={};

    if(txt==nullptr || txt[0]=='\0')
      continue;

    if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg.is(&player->inventory())){
      val = r.sellCost();
      }

    std::snprintf(vint,sizeof(vint),"%d",val);
    int tw = fnt.textSize(vint).w;

    fnt.drawText(p, x+20,  y+int(i+2)*fnt.pixelSize(), txt);
    if(val!=0)
      fnt.drawText(p,x+dw-tw-20,y+int(i+2)*fnt.pixelSize(),vint);
    }

  const int sz=dh;
  renderer.drawItem(x+dw-sz-sz/2,y,sz,sz,r);
  }

void InventoryMenu::draw(FrameBuffer& /*fbo*/, Tempest::Encoder<CommandBuffer>& cmd, uint8_t fId) {
  renderer.draw(cmd,fId);
  }
