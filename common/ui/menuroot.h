#pragma once

#include <Tempest/Widget>

#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>

#include "utils/keycodec.h"

class GameMenu;
class Npc;

class MenuRoot : public Tempest::Widget {
  public:
    MenuRoot(KeyCodec& keyCodec);
    ~MenuRoot() override;

    void setMainMenu();
    void setMenu(std::string_view menu, KeyCodec::Action key = KeyCodec::Escape);
    void setMenu(GameMenu* w);
    void pushMenu(GameMenu* w);
    void popMenu();
    void closeAll();
    bool isActive() const;
    void setPlayer(const Npc& pl);
    void processMusicTheme();

#ifdef OPENGOTHIC_TOUCH_UI
    bool isTouchMenu() const;
    void mobileTouch(float x, float y, int phase);
    void mobileTap(float x, float y);
    void mobileScroll(float dy);
#endif
    void showVersion(bool s);
    bool hasVersionLine() const;

    void mouseWheelEvent(Tempest::MouseEvent& event) override;
    void keyRepeatEvent (Tempest::KeyEvent&   event) override;
    void keyDownEvent   (Tempest::KeyEvent&   event) override;

  protected:
    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;
    void mouseDragEvent (Tempest::MouseEvent& event) override;

  private:
    void initSettings();

    std::unique_ptr<zenkit::DaedalusVm>    vm;
    int32_t                                vmLang = -1;

    GameMenu*                              current=nullptr;
    std::vector<std::unique_ptr<GameMenu>> menuStack;
    KeyCodec&                              keyCodec;

    Tempest::Event::KeyType                cheatCode[6] = {};
    bool                                   showVersionHint = false;
#if defined(__MOBILE_PLATFORM__)
    Tempest::Point                         touchStart;
    bool                                   touchMoved = false;
#endif
  };
