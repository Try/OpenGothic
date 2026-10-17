#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Sprite>

#include "graphics/inventoryrenderer.h"
#include "game/inventory.h"
#ifdef OPENGOTHIC_TOUCH_UI
#include "ui/mobilecontrolsmodel.h"
#endif

class Npc;
class Item;
class Inventory;
class Interactive;
class World;
class KeyCodec;

class InventoryMenu : public Tempest::Widget {
  public:
    InventoryMenu(const KeyCodec& key);
    ~InventoryMenu();

    enum class State:uint8_t {
      Closed=0,
      Equip,
      Chest,
      Trade,
      Ransack,
      LockPicking
      };

    enum class LootMode:uint8_t {
      Normal=0,
      Stack,
      Ten,
      Hundred
      };

    enum class DrawPass:uint8_t {
      Back,
      Front
      };

    /// Items of the inventory can be filtered by their kind
    enum Category : int {
      CatAll,
      CatWeapons,
      CatArmor,
      CatJewelry,
      CatMagic,
      CatFood,
      CatPotion,
      CatDocuments,
      CatMisc,
      CatCount
      };

    void  setCategory(int category);
    int   category() const;
    bool  hasSelectedItem();
    void  mobileScroll(float dy);
#ifdef OPENGOTHIC_TOUCH_UI
    void  mobileTap(float x, float y);
    void  mobileTouch(float x, float y, int phase);
    bool  mobileTwoLists() const { return pageOth!=nullptr; }
    std::string_view mobileActionLabel();
    void  setTouchSprites(const Tempest::Sprite& slot, const Tempest::Sprite& selected) {
      touchSlot     = slot;
      touchSelected = selected;
      }
#endif
    void  close();
    void  open(Npc& pl);
    void  trade(Npc& pl,Npc& tr);
    bool  ransack(Npc& pl,Npc& tr);
    void  open(Npc& pl,Interactive& chest);
    State isOpen() const;
    bool  isActive() const;
    /// Another screen (a document, a video) is shown over the inventory
    void  setCovered(bool c) { covered = c; }
    bool  isCovered() const { return covered; }
    void  onWorldChanged();

    void  tick(uint64_t dt);
    void  draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void  paintNumOverlay(Tempest::PaintEvent& e);

    void  keyDownEvent  (Tempest::KeyEvent&   e) override;
    void  keyRepeatEvent(Tempest::KeyEvent&   e) override;
    void  keyUpEvent    (Tempest::KeyEvent&   e) override;

  protected:
    void  paintEvent     (Tempest::PaintEvent& e) override;

    void  mouseDownEvent (Tempest::MouseEvent& event) override;
    void  mouseUpEvent   (Tempest::MouseEvent& event) override;
    void  mouseWheelEvent(Tempest::MouseEvent& event) override;

  private:
    struct Page;
    struct InvPage;
    struct TradePage;
    struct RansackPage;

    struct PageLocal final {
      size_t                  sel    = 0;
      size_t                  scroll = 0;
      float                   scrollPx = 0;   // touch lists are scrolled by pixels, scroll is the first visible row
      };

    const KeyCodec&           keycodec;

    const Tempest::Texture2d* tex =nullptr;
    const Tempest::Texture2d* slot=nullptr;
    const Tempest::Texture2d* selT=nullptr;
    const Tempest::Texture2d* selU=nullptr;

    State                     state      =State::Closed;
    Npc*                      player     =nullptr;
    Npc*                      trader     =nullptr;
    Interactive*              chest      =nullptr;

    std::unique_ptr<Page>     pageOth, pagePl;
    PageLocal                 pageLocal[2];

    uint8_t                   page       =0;
    bool                      covered    =false;
    Tempest::Timer            takeTimer;
    size_t                    takeCount  =0;
    LootMode                  lootMode   =LootMode::Normal;
    InventoryRenderer         renderer;

    size_t                    columsCount = 5;
    int32_t                   scrollDelta = 0;

#ifdef OPENGOTHIC_TOUCH_UI
    static constexpr size_t   TouchColumns = 3;
    Tempest::Sprite           touchSlot, touchSelected;

    /// Dragging of a list with a finger
    struct TouchDrag final {
      bool                    active      = false;
      uint8_t                 list        = 0;     // index of pageLocal
      float                   startY      = 0;
      float                   startScroll = 0;     // in pixels
      };
    Tempest::Rect             touchClip;
    TouchDrag                 touchDrag;

    PageLocal*                touchList(float x, float y, const Page*& list, uint8_t& index);
    float                     touchMaxScroll(const Page& list) const;

    float                     touchUnit() const;
    float                     touchTextScale() const; // text is larger than on desktop, it is read from a phone
    Mobile::InventoryLayout   touchLayout() const;
    bool                      touchHit(float x, float y, uint8_t& list, int& column, int& row) const;
    void                      drawAllTouch(Tempest::Painter& p, Npc& player, DrawPass pass);
    void                      drawTouchHeader(Tempest::Painter& p, std::string_view title, const Mobile::Rect& r);
    void                      drawScrollHints(Tempest::Painter& p, const Page& inv, const PageLocal& sel, const Mobile::Rect& r);
    void                      drawInfoTouch(Tempest::Painter& p);
#endif

    size_t                    rowsCount() const;

    Tempest::Size             slotSize() const;
    int                       infoHeight() const;
    size_t                    pagesCount() const;

    const Page&               activePage();
    PageLocal&                activePageSel();

    void          adjustScrollRows();
    void          processMove(Tempest::KeyEvent& e);
    void          moveLeft(bool usePage);
    void          moveRight(bool usePage);
    void          moveUp();
    void          moveDown();

    void          onItemAction(uint8_t slotHint);
    void          onTakeStuff();
    void          adjustScroll();
    void          drawAll   (Tempest::Painter& p, Npc& player, DrawPass pass);
    void          drawItems (Tempest::Painter& p, DrawPass pass, const Page &inv, const PageLocal &sel, int x, int y, int wcount, int hcount);
    void          drawSlot  (Tempest::Painter& p, DrawPass pass, const Inventory::Iterator& it,
                             const Page& page, const PageLocal &sel, int x, int y, size_t id);
    void          drawGold  (Tempest::Painter& p, Npc &player, int x, int y);
    void          drawHeader(Tempest::Painter& p, std::string_view title, int x, int y);
    void          drawInfo  (Tempest::Painter& p);
  };
