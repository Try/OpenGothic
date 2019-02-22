#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>

class Npc;
class World;

class InventoryMenu : public Tempest::Widget {
  public:
    InventoryMenu();

    enum class State:uint8_t {
      Closed=0,
      Equip
      };

    void  setWorld(const World* w);
    void  close();
    void  open();
    State isOpen() const;

  protected:
    void paintEvent     (Tempest::PaintEvent& e) override;
    void keyDownEvent   (Tempest::KeyEvent&   e) override;
    void keyUpEvent     (Tempest::KeyEvent&   e) override;

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

  private:
    const Tempest::Texture2d* tex =nullptr;
    const Tempest::Texture2d* slot=nullptr;
    const Tempest::Texture2d* selT=nullptr;
    const Tempest::Texture2d* selU=nullptr;

    State                     state      =State::Closed;
    size_t                    sel        =0;
    size_t                    scroll     =0;
    const World*              world      =nullptr;
    const size_t              columsCount=5;
    size_t                    rowsCount() const;

    Tempest::Size slotSize() const;
    int           infoHeight() const;

    void          adjustScroll();
    void          drawAll (Tempest::Painter& p, Npc& player);
    void          drawSlot(Tempest::Painter& p, Npc& player, int x, int y, size_t id);
    void          drawGold(Tempest::Painter& p, Npc &player, int x, int y);
    void          drawInfo(Tempest::Painter& p, Npc &player);
  };
