#include "game/inventory.h"
#include "ui/gamepadinventoryinput.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <stdexcept>

// Only item storage, UI geometry, fonts and world presence are boundaries. The
// real Inventory declaration and generated production methods below own all
// iteration, page indexing, layout, scrolling and gamepad selection behavior.
class Item {
  public:
    static constexpr uint8_t NSLOT=255;
    static constexpr int MAX_UI_ROWS=6;
    size_t id=0, amount=0, equipped=0;
    bool multi=false, gold=false;
    uint8_t spellSlot=NSLOT;
    size_t clsId() const { return id; }
    size_t count() const { return amount; }
    size_t equipCount() const { return equipped; }
    bool isEquipped() const { return equipped!=0; }
    bool isMulti() const { return multi; }
    bool isGold() const { return gold; }
    uint8_t slot() const { return spellSlot; }
};
Inventory::Inventory()=default;
Inventory::~Inventory()=default;
// Fixture inserts already ordered entries; item sorting is outside this merge.
void Inventory::sortItems() const {}
class Npc {};

struct Size { int w=0, h=0; Size(int w,int h):w(w),h(h) {} };
class InventoryMenu {
  public:
    enum class State:uint8_t { Closed, Equip, Chest, Trade, Ransack, LockPicking };
    struct Page;
    struct InvPage;
    struct TradePage;
    struct RansackPage;
    struct PageLocal { size_t sel=0, scroll=0; };
    struct Margins { int left=0, top=0, right=0, bottom=0; };
    int height=0;
    float scale=1;
    Margins safeArea;
    State state=State::Equip;
    Npc* player=nullptr;
    uint8_t page=0;
    std::unique_ptr<Page> pageOth, pagePl;
    PageLocal pageLocal[2];
    size_t columsCount=5;
    int h() const { return height; }
    Margins margins() const { return safeArea; }
    size_t rowsCount() const;
    int headerHeight() const;
    Size slotSize() const;
    int infoHeight() const;
    size_t pagesCount() const;
    const Page& activePage();
    PageLocal& activePageSel();
    void adjustScroll();
    bool gamepadSelection(GamepadInventoryInput::Selection&);
};
class Gothic {
  public:
    struct Options { int inventoryCellSize=40; };
    static float interfaceScale(const InventoryMenu* menu) { return menu->scale; }
    static Options& options() { static Options value; return value; }
};
class Resources {
  public:
    struct Font { int pixels; int pixelSize() const { return pixels; } };
    static Font font(float scale) { return {int(16*scale)}; }
};

#include "inventory-merge-bodies.inc"

namespace {
Item& add(Inventory& inventory,size_t id,size_t count,size_t equipped=0,bool multi=false) {
  auto item=std::make_unique<Item>();
  item->id=id;
  item->amount=count;
  item->equipped=equipped;
  item->multi=multi;
  auto& result=*item;
  inventory.items.push_back(std::move(item));
  return result;
}

class InventoryMerge : public testing::Test {
  protected:
    Inventory inventory;
    Npc npc;
    InventoryMenu menu;
    void SetUp() override {
      Gothic::options().inventoryCellSize=40;
      menu.player=&npc;
      menu.pagePl=std::make_unique<InventoryMenu::InvPage>(inventory);
    }
    void expectSelection(size_t index,size_t cls,size_t count,bool equipped,uint8_t slot=Item::NSLOT) {
      menu.activePageSel().sel=index;
      GamepadInventoryInput::Selection selection;
      ASSERT_TRUE(menu.gamepadSelection(selection));
      EXPECT_EQ(selection.state,uint8_t(menu.state));
      EXPECT_EQ(selection.panel,menu.page);
      EXPECT_EQ(selection.index,index);
      EXPECT_EQ(selection.cls,cls);
      EXPECT_EQ(selection.count,count);
      EXPECT_EQ(selection.equipped,equipped);
      EXPECT_EQ(selection.slot,slot);
    }
};
}

TEST_F(InventoryMerge, TwoOrThreeIdenticalRingsKeepEquippedEntriesSeparate) {
  for(size_t rings : {2u,3u}) {
    inventory.items.clear();
    add(inventory,100,rings,2);
    add(inventory,200,7);
    const auto& page=menu.activePage();
    ASSERT_EQ(page.size(),rings+1);
    auto it=inventory.iterator(Inventory::T_Inventory);
    for(size_t i=0;i<rings;++i) {
      ASSERT_TRUE(it.isValid());
      EXPECT_EQ(it->clsId(),100u);
      EXPECT_EQ(it.count(),1u);
      EXPECT_EQ(it.isEquipped(),i<2);
      expectSelection(i,100,1,i<2);
      const auto fromPage=page.get(i);
      EXPECT_EQ(fromPage.count(),1u);
      EXPECT_EQ(fromPage.isEquipped(),i<2);
      ++it;
    }
    ASSERT_TRUE(it.isValid());
    EXPECT_EQ(it->clsId(),200u);
    EXPECT_EQ(it.count(),7u);
    EXPECT_FALSE(it.isEquipped());
    expectSelection(rings,200,7,false);
    ++it;
    EXPECT_FALSE(it.isValid());
    EXPECT_FALSE(page.get(page.size()).isValid());
  }
}

TEST_F(InventoryMerge, MultiStackIsOneEntryAndUnequippedRemainderIsOneEntry) {
  add(inventory,100,6,2);
  add(inventory,200,17,1,true).spellSlot=4;
  add(inventory,300,9,0,true);
  ASSERT_EQ(menu.activePage().size(),5u);
  expectSelection(0,100,1,true);
  expectSelection(1,100,1,true);
  expectSelection(2,100,4,false);
  expectSelection(3,200,17,true,4);
  expectSelection(4,300,9,false);
  EXPECT_FALSE(menu.activePage().get(5).isValid());
}

TEST_F(InventoryMerge, PendingQuantityDistinguishesIdenticalEquippedAndRemainingEntries) {
  auto& rings=add(inventory,100,5,2);
  GamepadInventoryInput input;
  GamepadInventoryInput::Selection selected, current;
  menu.activePageSel().sel=0;
  ASSERT_TRUE(menu.gamepadSelection(selected));
  ASSERT_TRUE(input.beginQuantity(selected));
  EXPECT_TRUE(input.matches(selected));
  menu.activePageSel().sel=1;
  ASSERT_TRUE(menu.gamepadSelection(current));
  EXPECT_FALSE(input.matches(current)); // same class/count/equipped, another index
  menu.activePageSel().sel=2;
  ASSERT_TRUE(menu.gamepadSelection(current));
  EXPECT_FALSE(input.matches(current));
  ASSERT_TRUE(input.beginQuantity(current));
  input.process(KeyCodec::UiNext);
  EXPECT_EQ(input.quantity(),3u);
  rings.amount=4;
  ASSERT_TRUE(menu.gamepadSelection(selected));
  EXPECT_FALSE(input.matches(selected)); // same index/class, changed remainder
  rings.equipped=3;
  ASSERT_TRUE(menu.gamepadSelection(selected));
  EXPECT_TRUE(selected.equipped);
  EXPECT_FALSE(input.matches(selected));
}

TEST_F(InventoryMerge, TradeRansackAndClosedPagesRetainVisibilityRules) {
  add(inventory,100,3,2);
  add(inventory,200,9,0,true).gold=true;
  add(inventory,300,4);
  menu.pageOth=std::make_unique<InventoryMenu::TradePage>(inventory);
  ASSERT_EQ(menu.activePage().size(),1u);
  expectSelection(0,300,4,false);
  menu.pageOth=std::make_unique<InventoryMenu::RansackPage>(inventory);
  ASSERT_EQ(menu.activePage().size(),2u);
  expectSelection(0,200,9,false);
  menu.page=1;
  expectSelection(0,100,1,true);
  for(auto state : {InventoryMenu::State::Closed,InventoryMenu::State::LockPicking}) {
    menu.state=state;
    GamepadInventoryInput::Selection selection;
    EXPECT_FALSE(menu.gamepadSelection(selection));
  }
  menu.state=InventoryMenu::State::Equip;
  menu.player=nullptr;
  GamepadInventoryInput::Selection selection;
  EXPECT_FALSE(menu.gamepadSelection(selection));
  menu.player=&npc;
  menu.pagePl=std::make_unique<InventoryMenu::Page>();
  EXPECT_EQ(menu.activePage().size(),0u);
  // Empty Page has no iterator; the ordinary empty inventory page does.
  menu.pagePl=std::make_unique<InventoryMenu::InvPage>(InventoryMerge::inventory);
  menu.activePageSel().sel=100;
  EXPECT_FALSE(menu.gamepadSelection(selection));
}

TEST_F(InventoryMerge, ScaledHeaderAndAsymmetricSafeAreaDetermineActualRows) {
  // Explicit expected geometry: scale .5 => header17/info69/cell20;
  // scale 2 => header68/info276/cell80. Both safe edges affect usable space.
  menu.safeArea={19,130,47,90};
  menu.height=700;
  menu.scale=.5f;
  EXPECT_EQ(menu.headerHeight(),17);
  EXPECT_EQ(menu.infoHeight(),69);
  EXPECT_EQ(menu.slotSize().h,20);
  EXPECT_EQ(menu.rowsCount(),18u);
  menu.scale=2;
  EXPECT_EQ(menu.headerHeight(),68);
  EXPECT_EQ(menu.infoHeight(),276);
  EXPECT_EQ(menu.rowsCount(),1u);
  menu.height=1000;
  EXPECT_EQ(menu.rowsCount(),5u);
  menu.safeArea.top=20;
  menu.safeArea.bottom=5;
  EXPECT_EQ(menu.rowsCount(),6u);
}

TEST_F(InventoryMerge, ScrollTerminatesAndKeepsSelectionVisibleAtAllViewportSizes) {
  for(size_t i=0;i<80;++i)
    add(inventory,100+i,1);
  menu.safeArea={7,130,61,90};
  for(float scale : {.01f,.5f,2.f})
    for(int height : {1,700,1000}) {
      SCOPED_TRACE(testing::Message()<<scale<<'/'<<height);
      menu.scale=scale;
      menu.height=height;
      if(height==1)
        EXPECT_EQ(menu.rowsCount(),1u);
      for(size_t selection : {79u,0u,38u,500u}) {
        menu.activePageSel().sel=selection;
        menu.adjustScroll();
        const auto& local=menu.activePageSel();
        EXPECT_EQ(local.sel,std::min(selection,size_t(79)));
        EXPECT_GE(local.sel,local.scroll*menu.columsCount);
        EXPECT_LT(local.sel,(local.scroll+menu.rowsCount())*menu.columsCount);
      }
    }
  inventory.items.clear();
  menu.activePageSel()={500,20};
  menu.adjustScroll();
  EXPECT_EQ(menu.activePageSel().sel,0u);
  EXPECT_EQ(menu.activePageSel().scroll,0u);
}
