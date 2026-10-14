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
    DialogMenu(InventoryMenu& trade);
    ~DialogMenu();

    void tick(uint64_t dt);
    void clear();

    void onWorldChanged();

    bool isMobsiDialog() const;
    void dialogCamera(Camera& camera);

    // gothic-diablo: cinematic dialogs in the ARPG mode: a talk between two is filmed by the camera's director
    // (letterbox, subtitles and the answers in the lower bar, nothing else over the picture)
    bool                 isArpgCinema() const;
    ArpgDialogCam::Input arpgDialogInput() const;
    // a third one speaks in a cinematic talk (his line comes outside the dialog): subtitles, the camera goes to him
    bool                 arpgThirdLine(Npc& npc, std::string_view text, uint64_t ms);

    void openPipe(Npc &player, Npc& npc, AiOuputPipe*& out);
    bool isNpcInDialog(const Npc* npc) const;

    bool isActive() const;
    bool hasContent() const;

    void print      (std::string_view msg);
    void printScreen(std::string_view msg, int x, int y, int time, const GthFont &font);
    void drawMsg    (Tempest::Painter& p, int offsetY);

    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

  protected:
    void paintEvent (Tempest::PaintEvent& e) override;
    void paintChoice(Tempest::PaintEvent& e);

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseMoveEvent (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

    void onSelect();
    void setupSettings();
    bool isChoiceMenuActive() const;

  private:
    const Tempest::Texture2d* tex    =nullptr;
    const Tempest::Texture2d* ambient=nullptr;

    struct Pipe : AiOuputPipe {
      Pipe(DialogMenu& owner):owner(owner){}

      bool output   (Npc& npc, std::string_view text) override;
      bool outputSvm(Npc& npc, std::string_view text) override;
      bool outputOv (Npc& npc, std::string_view text) override;
      bool printScr (Npc& npc, int time, std::string_view msg, int x, int y, std::string_view font) override;

      bool close() override;
      bool isFinished() override;

      DialogMenu& owner;
      };

    enum {
      MAX_PRINT = 3,
      ANIM_TIME = 200,
      };

    enum class State : uint8_t {
      Idle    =0,
      PreStart=1,
      Active  =2
      };

    struct Entry {
      std::string txt;
      uint64_t    msgTime = 0;
      uint64_t    time    = 0;
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
    void onEntry(const GameScript::DlgChoice& e);
    void onDoneText();
    void close();
    bool aiOutput  (Npc& npc, std::string_view msg);
    bool aiPrintScr(Npc& npc, int time, std::string_view msg, int x, int y, std::string_view font);
    bool aiClose();

    bool haveToWaitOutput() const;
    bool hasPrintMsg() const;
    bool haveToShowSubtitles(bool isPl) const;

    void drawTextMultiline(Tempest::Painter& p, int x, int y, int w, int h, std::string_view txt, bool isPl);
    Tempest::Size processTextMultiline(Tempest::Painter* p, int x, int y, int w, int h, std::string_view txt, bool isPl);

    void startTrade();
    void skipPhrase();

    // gothic-diablo: cinematic look (ARPG mode)
    void paintArpg(Tempest::Painter& p);
    void paintScreenMessages(Tempest::Painter& p);
    bool arpgChoiceReady() const;

    InventoryMenu&                      trade;
    Pipe                                pipe;

    Tempest::SoundDevice                soundDevice;

    std::vector<GameScript::DlgChoice>  choice;
    GameScript::DlgChoice               selected;
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

    bool                                dlgAnimation        = true;
    bool                                showSubtitles       = true;
    bool                                showSubtitlesPlayer = true;
    uint64_t                            choiceAnimTime      = 0;

    uint32_t                            lineId              = 0; // gothic-diablo: counts the lines (cuts of the camera)

    // gothic-diablo: acting in cinematic dialogs: looks, nods of the listener, moods, blinking
    struct Acting {
      bool     on        = false;
      uint64_t t         = 0;    // ms in the talk
      uint32_t line      = 0;
      Npc*     nodWho    = nullptr;
      uint64_t nodAt     = 0;
      uint64_t lastNodPl = 0, lastNodOther = 0;
      uint64_t blinkPl   = 0, blinkOther   = 0;
      uint64_t moodAt    = 0;
      const char* mood   = nullptr;
      };
    Acting                              acting;
    void tickActing(uint64_t dt);
    void stopActing();
    float                               barsK               = 0; // gothic-diablo: 0..1 letterbox
    std::vector<Tempest::Rect>          choiceRects;             // gothic-diablo: the answers on the screen (mouse)
    Npc*                                third     = nullptr;     // gothic-diablo: a third one who speaks in the talk
    std::string                         thirdTxt;
    uint64_t                            thirdTime = 0;
    uint64_t                            thirdLeft = 0;
  };
