#pragma once

#include <Tempest/Font>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Widget>

#include "world/worldscript.h"

class Npc;

class DialogMenu : public Tempest::Widget {
  public:
    DialogMenu(Gothic& gothic);

    void tick(uint64_t dt);
    void aiOutput(const char* msg);
    void aiClose();
    void start(std::vector<WorldScript::DlgChoise>&& choise,Npc& pl,Npc& other);

    void printScreen(const char* msg,int x,int y,int time,const Tempest::Font& font);

  protected:
    void paintEvent (Tempest::PaintEvent& e) override;
    void paintChoise(Tempest::PaintEvent& e);

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

    void keyDownEvent   (Tempest::KeyEvent&   event) override;

  private:
    const Tempest::Texture2d* tex=nullptr;
    enum Flags:uint8_t {
      NoFlags =0,
      DlgClose
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

    void onEntry(const WorldScript::DlgChoise& e);
    void onEntry(const Entry& e);
    void onDoneText();

    Gothic&                             gothic;
    std::vector<WorldScript::DlgChoise> choise;
    WorldScript::DlgChoise              selected;
    Npc*                                pl   =nullptr;
    Npc*                                other=nullptr;
    size_t                              dlgSel=0;

    std::vector<Entry>                  txt;
    std::vector<PScreen>                pscreen;
    uint64_t                            remDlg=0;
  };
