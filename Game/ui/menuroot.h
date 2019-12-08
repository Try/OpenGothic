#pragma once

#include <Tempest/Widget>
#include <daedalus/DaedalusVM.h>

class Gothic;
class GameMenu;
class Npc;

class MenuRoot : public Tempest::Widget {
  public:
    MenuRoot(Gothic& gothic);
    ~MenuRoot() override;

    void setMenu(const char* menu);
    void setMenu(GameMenu* w);
    void pushMenu(GameMenu* w);
    void popMenu();
    void closeAll();
    void setPlayer(const Npc& pl);

    void mouseWheelEvent(Tempest::MouseEvent& event) override;

  protected:
    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;

    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

  private:
    Gothic&                                gothic;
    std::unique_ptr<Daedalus::DaedalusVM>  vm;
    GameMenu*                              current=nullptr;
    std::vector<std::unique_ptr<GameMenu>> menuStack;
  };
