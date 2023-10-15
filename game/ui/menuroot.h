#pragma once

#include <Tempest/Widget>

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

#include "utils/keycodec.h"

class GameMenu;
class Npc;

class MenuRoot : public Tempest::Widget {
  public:
    MenuRoot(KeyCodec& keyCodec);
    ~MenuRoot() override;

    void setMainMenu();
    void setMenu(const char* menu, KeyCodec::Action key = KeyCodec::Escape);
    void setMenu(GameMenu* w);
    void pushMenu(GameMenu* w);
    void popMenu();
    void closeAll();
    bool isActive() const;
    void setPlayer(const Npc& pl);
    void processMusicTheme();

    void showVersion(bool s);
    bool hasVersionLine() const;

    void mouseWheelEvent(Tempest::MouseEvent& event) override;
    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

  protected:
    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;

  private:
    void initSettings();

    std::unique_ptr<phoenix::vm>           vm;
    int32_t                                vmLang = -1;

    GameMenu*                              current=nullptr;
    std::vector<std::unique_ptr<GameMenu>> menuStack;
    KeyCodec&                              keyCodec;

    Tempest::Event::KeyType                cheatCode[6] = {};
    bool                                   showVersionHint = false;
  };
