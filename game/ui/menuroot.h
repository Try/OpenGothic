#pragma once

#include <Tempest/Widget>
#include <daedalus/DaedalusVM.h>

#include "utils/keycodec.h"

class GameMenu;
class Npc;

class MenuRoot : public Tempest::Widget {
  public:
    MenuRoot(KeyCodec& keyCodec);
    ~MenuRoot() override;

    void setMenu(const char* menu, KeyCodec::Action key = KeyCodec::Escape);
    void setMenu(GameMenu* w);
    void pushMenu(GameMenu* w);
    void popMenu();
    void closeAll();
    bool isActive() const;
    void setPlayer(const Npc& pl);
    void processMusicTheme();

    void mouseWheelEvent(Tempest::MouseEvent& event) override;
    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

  protected:
    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;

  private:
    std::unique_ptr<Daedalus::DaedalusVM>  vm;
    GameMenu*                              current=nullptr;
    std::vector<std::unique_ptr<GameMenu>> menuStack;
    KeyCodec&                              keyCodec;
  };
