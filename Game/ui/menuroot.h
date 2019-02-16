#pragma once

#include <Tempest/Widget>

class Gothic;
class GameMenu;
class Npc;

class MenuRoot : public Tempest::Widget {
  public:
    MenuRoot(Gothic& gothic);
    ~MenuRoot() override;

    void setMenu(GameMenu* w);
    void pushMenu(GameMenu* w);
    void popMenu();
    void setPlayer(const Npc& pl);

    void mouseWheelEvent(Tempest::MouseEvent& event) override;

  protected:
    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;

    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

  private:
    Gothic&                                gothic;
    GameMenu*                              current=nullptr;
    std::vector<std::unique_ptr<GameMenu>> menuStack;
  };
