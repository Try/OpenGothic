#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>
#include <Tempest/Timer>

#include "graphics/inventoryrenderer.h"
#include "game/inventory.h"

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

    void  close();
    void  open(Npc& pl);
    void  trade(Npc& pl,Npc& tr);
    bool  ransack(Npc& pl,Npc& tr);
    void  open(Npc& pl,Interactive& chest);
    State isOpen() const;
    bool  isActive() const;
    void  onWorldChanged();

    void  tick(uint64_t dt);
    void  draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void  paintNumOverlay(Tempest::PaintEvent& e);

    void  keyDownEvent  (Tempest::KeyEvent&   e) override;
    void  keyRepeatEvent(Tempest::KeyEvent&   e) override;
    void  keyUpEvent    (Tempest::KeyEvent&   e) override;

  protected:
    void paintEvent     (Tempest::PaintEvent& e) override;

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

  private:
    struct Page;
    struct InvPage;
    struct TradePage;
    struct RansackPage;

    struct PageLocal final {
      size_t                  sel    = 0;
      size_t                  scroll = 0;
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
    Tempest::Timer            takeTimer;
    size_t                    takeCount  =0;
    LootMode                  lootMode   =LootMode::Normal;
    InventoryRenderer         renderer;

    size_t                    columsCount=5;

    size_t                    rowsCount() const;

    Tempest::Size             slotSize() const;
    int                       infoHeight() const;
    size_t                    pagesCount() const;

    const Page&               activePage();
    PageLocal&                activePageSel();
    const World*              world() const;

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
