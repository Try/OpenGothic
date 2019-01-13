#pragma once

#include <Tempest/Widget>

class Gothic;
class GameMenu;

class MenuRoot : public Tempest::Widget {
  public:
    MenuRoot(Gothic& gothic);
    ~MenuRoot() override;

    void setMenu(GameMenu* w);
    void pushMenu(GameMenu* w);
    void popMenu();

  protected:
    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

  private:
    Gothic&                                gothic;
    GameMenu*                              current=nullptr;
    std::vector<std::unique_ptr<GameMenu>> menuStack;
  };
