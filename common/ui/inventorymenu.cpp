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
  cancelGamepadInput();
  if(state!=State::Closed) {
    if(state==State::Trade)
      Gothic::inst().emitGlobalSound("TRADE_CLOSE"); else
      Gothic::inst().emitGlobalSound("INV_CLOSE");
    }
  renderer.reset(true);
  takeTimer.stop();
  if(state!=State::Closed)
    ++inputRevisionCounter;
  state  = State::Closed;
  }

void InventoryMenu::open(Npc &pl) {
  if(pl.isDown() || pl.isMonster() || pl.isInAir() || pl.isSlide() || (pl.interactive()!=nullptr))
    return;
  if(pl.bodyStateMasked()==BS_UNCONSCIOUS || pl.bodyStateMasked()==BS_LIE)
    return;
  cancelGamepadInput();
  if(pl.weaponState()!=WeaponState::NoWeapon) {
    pl.stopAnim("");
    pl.closeWeapon(false);
    }
  ++inputRevisionCounter;
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
  cancelGamepadInput();
  ++inputRevisionCounter;
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
  cancelGamepadInput();
  ++inputRevisionCounter;
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
  cancelGamepadInput();

  ++inputRevisionCounter;
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
      ++inputRevisionCounter;
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

  if(gamepadInput.mode()!=GamepadInventoryInput::Mode::Browse) {
    auto action = keycodec.tr(e);
    if(e.key==KeyEvent::K_Return)
      action = KeyCodec::ActionGeneric;
    if(e.key==KeyEvent::K_ESCAPE || action==KeyCodec::Inventory)
      action = KeyCodec::Escape;
    processGamepadModal(action);
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
  if(gamepadInput.mode()!=GamepadInventoryInput::Mode::Browse) {
    const auto action = keycodec.tr(e);
    if(action==KeyCodec::Forward || action==KeyCodec::Back || action==KeyCodec::Left || action==KeyCodec::Right)
      processGamepadModal(action);
    return;
    }
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

  if(gamepadInput.mode()!=GamepadInventoryInput::Mode::Browse) {
    if(e.button==MouseEvent::ButtonLeft)
      processGamepadModal(KeyCodec::ActionGeneric);
    else if(e.button==MouseEvent::ButtonRight)
      processGamepadModal(KeyCodec::Escape);
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

  if(state==State::LockPicking)
    return;

  if(gamepadInput.mode()!=GamepadInventoryInput::Mode::Browse) {
    if(e.delta!=0)
      processGamepadModal(e.delta>0 ? KeyCodec::Forward : KeyCodec::Back);
    return;
    }

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
  return size_t(std::max(1,(h()-iy-infoHeight()-20)/std::max(1,slotSize().h)));
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
  drawGamepadModal(p);
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
  
void InventoryMenu::onItemAction(uint8_t slotHint, bool repeat) {
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
    if(repeat) {
      lootMode = LootMode::Normal;
      takeTimer.start(200);
      onTakeStuff();
      } else {
      transferItems(1);
      }
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

  transferItems(itemCount);
  }

void InventoryMenu::transferItems(size_t itemCount) {
  auto& page = activePage();
  auto& sel = activePageSel();
  auto it = page.get(sel.sel);
  if(!it.isValid())
    return;
  if(it.count() < itemCount)
    itemCount = it.count();

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

void InventoryMenu::drawHeader(Painter &p, std::string_view title, int x, int y) {
  const float scale = Gothic::interfaceScale(this);
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

void InventoryMenu::draw(Tempest::Encoder<CommandBuffer>& cmd) {
  renderer.draw(cmd);
  }

void InventoryMenu::onGamepad(KeyCodec::Action action) {
  if(state==State::Closed || state==State::LockPicking)
    return;
  if(gamepadInput.mode()!=GamepadInventoryInput::Mode::Browse) {
    processGamepadModal(action);
    return;
    }
  // A controller command must not leave a keyboard transfer timer acting on
  // the newly selected item or panel.
  takeTimer.stop();
  takeCount = 0;
  switch(action) {
    case KeyCodec::Forward: moveUp(); break;
    case KeyCodec::Back: moveDown(); break;
    case KeyCodec::Left: moveLeft(true); break;
    case KeyCodec::Right: moveRight(true); break;
    case KeyCodec::ActionGeneric: onItemAction(Item::NSLOT, false); break;
    case KeyCodec::UiPrevious: page = 0; break;
    case KeyCodec::UiNext: page = uint8_t(pagesCount()-1); break;
    case KeyCodec::UiAmount:
    case KeyCodec::UiAssign: {
      GamepadInventoryInput::Selection selection;
      if(!gamepadSelection(selection))
        return;
      if(action==KeyCodec::UiAssign) {
        const auto it = activePage().get(selection.index);
        if(state!=State::Equip || !it->isSpellOrRune())
          return;
        gamepadInput.beginSpell(selection);
        } else if(!gamepadInput.beginQuantity(selection)) {
        return;
        }
      takeTimer.stop();
      takeCount = 0;
      lootMode = LootMode::Normal;
      ++inputRevisionCounter;
      break;
      }
    case KeyCodec::Escape: close(); break;
    default: return;
    }
  adjustScroll();
  update();
  }

void InventoryMenu::cancelGamepadInput() {
  takeTimer.stop();
  takeCount = 0;
  lootMode = LootMode::Normal;
  if(gamepadInput.mode()==GamepadInventoryInput::Mode::Browse)
    return;
  gamepadInput.reset();
  ++inputRevisionCounter;
  update();
  }

bool InventoryMenu::gamepadSelection(GamepadInventoryInput::Selection& selection) {
  if(player==nullptr || state==State::Closed || state==State::LockPicking)
    return false;
  const auto& sel = activePageSel();
  const auto it = activePage().get(sel.sel);
  if(!it.isValid())
    return false;
  selection = {uint8_t(state),page,sel.sel,it->clsId(),it.count(),it.slot(),it.isEquipped()};
  return true;
  }

void InventoryMenu::processGamepadModal(KeyCodec::Action action) {
  GamepadInventoryInput::Selection selection;
  if(!gamepadSelection(selection) || !gamepadInput.matches(selection)) {
    cancelGamepadInput();
    return;
    }
  const auto result = gamepadInput.process(action);
  if(result==GamepadInventoryInput::Result::Cancel) {
    cancelGamepadInput();
    return;
    }
  if(result==GamepadInventoryInput::Result::Confirm) {
    const auto mode = gamepadInput.mode();
    const auto amount = gamepadInput.quantity();
    const auto slot = gamepadInput.slot();
    cancelGamepadInput();
    if(mode==GamepadInventoryInput::Mode::Quantity)
      transferItems(amount);
    else
      onItemAction(slot,false);
    adjustScroll();
    }
  update();
  }

void InventoryMenu::drawGamepadModal(Painter& p) {
  if(gamepadInput.mode()==GamepadInventoryInput::Mode::Browse)
    return;
  GamepadInventoryInput::Selection selection;
  if(!gamepadSelection(selection) || !gamepadInput.matches(selection)) {
    cancelGamepadInput();
    return;
    }
  const float scale = Gothic::interfaceScale(this);
  auto fnt = Resources::font(scale);
  const bool russian = Gothic::settingsGetI("GAME","language")==int32_t(ScriptLang::RU);
  // Gothic's bitmap fonts consume game-codepage bytes, not UTF-8.
  const auto local = [russian](const char* en, const char* ru) { return russian ? ru : en; };
  const char* operation = local("Assign slot","\xcd\xe0\xe7\xed\xe0\xf7\xe8\xf2\xfc \xf1\xeb\xee\xf2");
  if(gamepadInput.mode()==GamepadInventoryInput::Mode::Quantity) {
    if(state==State::Equip)
      operation = local("Drop items","\xc2\xfb\xe1\xf0\xee\xf1\xe8\xf2\xfc \xef\xf0\xe5\xe4\xec\xe5\xf2\xfb");
    else if(state==State::Trade)
      operation = activePage().is(&player->inventory()) ? local("Sell items","\xcf\xf0\xee\xe4\xe0\xf2\xfc \xef\xf0\xe5\xe4\xec\xe5\xf2\xfb") : local("Buy items","\xca\xf3\xef\xe8\xf2\xfc \xef\xf0\xe5\xe4\xec\xe5\xf2\xfb");
    else
      operation = activePage().is(&player->inventory()) ? local("Store items","\xcf\xee\xeb\xee\xe6\xe8\xf2\xfc \xef\xf0\xe5\xe4\xec\xe5\xf2\xfb") : local("Take items","\xc2\xe7\xff\xf2\xfc \xef\xf0\xe5\xe4\xec\xe5\xf2\xfb");
    }
  const auto item = activePage().get(selection.index);
  const auto value = gamepadInput.mode()==GamepadInventoryInput::Mode::Quantity ?
                     gamepadInput.quantity() : size_t(gamepadInput.slot());
  const char* controls = local("Confirm: Cross / Enter\nCancel: Circle / Esc","\xcf\xf0\xe8\xed\xff\xf2\xfc: Cross / Enter\n\xce\xf2\xec\xe5\xed\xe0: Circle / Esc");
  const char* change = gamepadInput.mode()==GamepadInventoryInput::Mode::Quantity ?
                      local("Left/Right: 1   Up/Down: 10\nL1: 1   R1: All","\xc2\xeb\xe5\xe2\xee/\xe2\xef\xf0\xe0\xe2\xee: 1   \xc2\xe2\xe5\xf0\xf5/\xe2\xed\xe8\xe7: 10\nL1: 1   R1: \xe2\xe5\xf1\xfc \xf1\xf2\xe5\xea") :
                      local("Arrows / L1 / R1: slot 3-10","\xd1\xf2\xf0\xe5\xeb\xea\xe8 / L1 / R1: \xf1\xeb\xee\xf2 3-10");
  const string_frm body(operation,": ",value,"\n",item->displayName(),"\n",change,"\n",controls);
  const int width = std::max(1,std::min(w()-20,fnt.textSize(body).w+40));
  const int padding = std::min(20,width/4);
  const int textWidth = std::max(1,width-2*padding);
  const int maxHeight = std::max(1,h()-20);
  int textHeight = fnt.textSize(textWidth,body).h;
  if(textHeight+2*padding>maxHeight) {
    const float fit = float(std::max(1,maxHeight-2*padding))/float(std::max(1,textHeight));
    fnt.setScale(scale*fit);
    textHeight = fnt.textSize(textWidth,body).h;
    }
  const int height = std::min(maxHeight,textHeight+2*padding);
  const int x = (w()-width)/2, y = (h()-height)/2;
  p.pushState();
  p.setScissor(x,y,width,height);
  p.setBrush(Color(0.04f,0.04f,0.04f,1));
  p.drawRect(x,y,width,height);
  fnt.drawText(p,x+padding,y+padding+fnt.pixelSize(),textWidth,
               std::max(1,height-2*padding),body,Tempest::NoAlign);
  p.popState();
  }
