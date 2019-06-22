#pragma once

#include <Tempest/Font>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Widget>
#include <Tempest/SoundEffect>
#include <Tempest/SoundDevice>

#include "world/worldscript.h"
#include "camera.h"

class Npc;
class Interactive;
class InventoryMenu;

class DialogMenu : public Tempest::Widget {
  public:
    DialogMenu(Gothic& gothic,InventoryMenu& trade);

    void tick(uint64_t dt);
    void clear();

    void onWorldChanged();

    const Camera& dialogCamera();

    void aiProcessInfos(Npc &player, Npc& npc);
    void aiOutput(Npc& npc, const char* msg, bool &done);
    void aiOutputForward(Npc& npc, const char* msg);
    void aiClose(bool& ret);
    void aiIsClose(bool& ret);
    bool isActive() const;
    bool start(Npc& pl,Npc& other);
    bool start(Npc& pl,Interactive& other);

    void print      (const char* msg);
    void printScreen(const char* msg,int x,int y,int time,const Tempest::Font& font);

    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

  protected:
    void paintEvent (Tempest::PaintEvent& e) override;
    void paintChoise(Tempest::PaintEvent& e);

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

    void onSelect();

  private:
    const Tempest::Texture2d* tex    =nullptr;
    const Tempest::Texture2d* ambient=nullptr;

    enum {
      MAX_PRINT=5
      };

    enum class State : uint8_t {
      Idle    =0,
      PreStart=1,
      Active  =2
      };

    struct Entry {
      std::string txt;
      uint32_t    time=0;
      };

    struct Forward {
      std::string txt;
      Npc*        npc=nullptr;
      };

    struct PScreen {
      std::string   txt;
      Tempest::Font font;
      uint32_t      time=0;
      int           x=-1;
      int           y=-1;
      };

    bool onStart(Npc& pl,Npc& other);
    void onEntry(const WorldScript::DlgChoise& e);
    void onDoneText();
    void close();
    bool haveToWaitOutput() const;
    void invokeMobsiState();

    void drawTextMultiline(Tempest::Painter& p, int x, int y, int w, int h, const std::string& txt, bool isPl);

    void startTrade();

    Gothic&                             gothic;
    InventoryMenu&                      trade;

    std::vector<WorldScript::DlgChoise> choise;
    WorldScript::DlgChoise              selected;
    Npc*                                pl   =nullptr;
    Npc*                                other=nullptr;
    size_t                              dlgSel=0;
    uint32_t                            depth=0;
    std::vector<uint32_t>               except;

    State                               state=State::Idle;
    Entry                               current;
    Tempest::SoundEffect                currentSnd;
    bool                                curentIsPl=false;
    bool                                dlgTrade=false;
    std::vector<Forward>                forwardText;
    int32_t                             mobsiState=-1;

    std::vector<PScreen>                pscreen;
    PScreen                             printMsg[MAX_PRINT];
    uint32_t                            remPrint=0;
    Camera                              camera;

    Tempest::SoundDevice                soundDevice;
  };
