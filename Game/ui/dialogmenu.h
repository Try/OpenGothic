#pragma once

#include <Tempest/Font>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Widget>
#include <Tempest/SoundEffect>
#include <Tempest/SoundDevice>

#include "game/gamescript.h"
#include "camera.h"

class Npc;
class Interactive;
class InventoryMenu;
class GthFont;

class DialogMenu : public Tempest::Widget {
  public:
    DialogMenu(Gothic& gothic,InventoryMenu& trade);

    void tick(uint64_t dt);
    void clear();

    void onWorldChanged();

    const Camera& dialogCamera();

    void openPipe(Npc &player, Npc& npc, AiOuputPipe*& out);

    void aiIsClose(bool& ret);
    bool isActive() const;

    void print      (const char* msg);
    void printScreen(const char* msg, int x, int y, int time, const GthFont &font);

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

    struct Pipe : AiOuputPipe {
      Pipe(DialogMenu& owner):owner(owner){}

      bool output   (Npc &npc, const Daedalus::ZString& text) override;
      bool outputSvm(Npc& npc, const Daedalus::ZString& text, int voice) override;
      bool outputOv (Npc& npc, const Daedalus::ZString& text, int voice) override;

      bool close() override;
      bool isFinished() override;

      DialogMenu& owner;
      };

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
      uint64_t    time=0;
      };

    struct Forward {
      std::string txt;
      Npc*        npc=nullptr;
      };

    struct PScreen {
      std::string    txt;
      const GthFont* font=nullptr;
      uint64_t       time=0;
      int            x=-1;
      int            y=-1;
      };

    bool onStart(Npc& pl,Npc& other);
    void onEntry(const GameScript::DlgChoise& e);
    void onDoneText();
    void close();
    bool aiOutput(Npc& npc, const Daedalus::ZString& msg);
    bool aiClose();

    bool haveToWaitOutput() const;

    void drawTextMultiline(Tempest::Painter& p, int x, int y, int w, int h, const std::string& txt, bool isPl);

    void startTrade();

    Gothic&                             gothic;
    InventoryMenu&                      trade;
    Pipe                                pipe;

    Tempest::SoundDevice                soundDevice;

    std::vector<GameScript::DlgChoise>  choise;
    GameScript::DlgChoise               selected;
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
    int32_t                             mobsiState=-1;

    std::vector<PScreen>                pscreen;
    PScreen                             printMsg[MAX_PRINT];
    uint64_t                            remPrint=0;
    Camera                              camera;
  };
