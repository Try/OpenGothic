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
    void keyDownEvent(Tempest::KeyEvent&   e) override;
    void keyUpEvent  (Tempest::KeyEvent&   e) override;
    void paintEvent  (Tempest::PaintEvent& e) override;

  private:
    const Tempest::Texture2d* tex =nullptr;
    const Tempest::Texture2d* slot=nullptr;
    const Tempest::Texture2d* selT=nullptr;

    State                     state=State::Closed;
    size_t                    sel =0;
    const World*              world=nullptr;
    const size_t              columsCount=5;

    Tempest::Size slotSize() const;
    int           infoHeight() const;

    void          drawAll (Tempest::Painter& p, Npc& player);
    void          drawSlot(Tempest::Painter& p, Npc& player, int x, int y, size_t id);
    void          drawGold(Tempest::Painter& p, Npc &player, int x, int y);
    void          drawInfo(Tempest::Painter& p, Npc &player);
  };
