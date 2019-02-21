#pragma once

#include <Tempest/Font>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Widget>

#include "world/worldscript.h"

class Npc;
class Interactive;

class DialogMenu : public Tempest::Widget {
  public:
    DialogMenu(Gothic& gothic);

    void tick(uint64_t dt);

    void aiProcessInfos(Npc &player, Npc& npc);
    void aiOutput(const char* msg);
    void aiClose();
    void aiIsClose(bool& ret);
    bool isActive() const;
    bool start(Npc& pl,Npc& other);
    bool start(Npc& pl,Interactive& other);

    void print      (const char* msg);
    void printScreen(const char* msg,int x,int y,int time,const Tempest::Font& font);

  protected:
    void paintEvent (Tempest::PaintEvent& e) override;
    void paintChoise(Tempest::PaintEvent& e);

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

    void keyDownEvent   (Tempest::KeyEvent&   event) override;

  private:
    const Tempest::Texture2d* tex    =nullptr;
    const Tempest::Texture2d* ambient=nullptr;

    enum Flags:uint8_t {
      NoFlags =0,
      DlgClose
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
      uint32_t    time=0;
      Flags       flag=NoFlags;
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
    void onEntry(const Entry& e);
    void onDoneText();
    void close();

    Gothic&                             gothic;
    std::vector<WorldScript::DlgChoise> choise;
    WorldScript::DlgChoise              selected;
    Npc*                                pl   =nullptr;
    Npc*                                other=nullptr;
    size_t                              dlgSel=0;
    uint32_t                            depth=0;

    State                               state=State::Idle;
    std::vector<Entry>                  txt;
    std::vector<PScreen>                pscreen;
    uint64_t                            remDlg=0;
    PScreen                             printMsg[MAX_PRINT];
    uint32_t                            remPrint=0;
  };
