#pragma once

#include <string>
#include <memory>
#include <thread>

#include <Tempest/Signal>
#include <daedalus/DaedalusVM.h>

#include "game/cameradefinitions.h"
#include "game/musicdefinitions.h"
#include "game/fightai.h"
#include "game/gamesession.h"
#include "world/world.h"
#include "ui/chapterscreen.h"

class Gothic final {
  public:
    Gothic(int argc,const char** argv);

    enum class LoadState:int {
      Idle    =0,
      Loading =1,
      Finalize=2
      };

    bool isGothic2() const;

    bool isInGame() const;
    bool doStartMenu() const { return !noMenu; }

    void         setGame(std::unique_ptr<GameSession> &&w);
    auto         clearGame() -> std::unique_ptr<GameSession>;
    const World* world() const;
    World*       world();
    WorldView*   worldView() const;
    Npc*         player();

    void      pushPause();
    void      popPause();
    bool      isPause() const;

    LoadState checkLoading();
    bool      finishLoading();
    void      startLoading(const std::function<void()> f);
    void      cancelLoading();

    void      tick(uint64_t dt);

    void      updateAnimation();

    auto      updateDialog(const WorldScript::DlgChoise& dlg, Npc& player, Npc& npc) -> std::vector<WorldScript::DlgChoise>;
    void      dialogExec  (const WorldScript::DlgChoise& dlg, Npc& player, Npc& npc);

    void      aiProcessInfos (Npc& player, Npc& npc);
    bool      aiOuput(Npc& player, const char* msg);
    void      aiForwardOutput(Npc& player, const char* msg);
    bool      aiCloseDialog();
    bool      aiIsDlgFinished();

    auto      getFightAi(size_t i) const -> const FightAi::FA&;
    auto      getMusicTheme(const char* name) -> const Daedalus::GEngineClasses::C_MusicTheme&;

    void      printScreen(const char* msg, int x, int y, int time, const Tempest::Font &font);
    void      print      (const char* msg);

    Tempest::Signal<void(const std::string&)>              onStartGame;
    Tempest::Signal<void(Npc&,Npc&)>                       onDialogProcess;
    Tempest::Signal<void(Npc&,const char*,bool&)>          onDialogOutput;
    Tempest::Signal<void(Npc&,const char*)>                onDialogForwardOutput;
    Tempest::Signal<void(bool&)>                           onDialogClose;
    Tempest::Signal<void(bool&)>                           isDialogClose;

    Tempest::Signal<void(const char*,int,int,int,const Tempest::Font&)> onPrintScreen;
    Tempest::Signal<void(const char*)>                                  onPrint;

    Tempest::Signal<void(const ChapterScreen::Show&)>                   onIntroChapter;

    const std::string&                    messageByName(const std::string &id) const;
    uint32_t                              messageTime(const std::string &id) const;
    const std::u16string&                 path() const { return gpath; }
    const std::string&                    defaultWorld() const;
    std::unique_ptr<Daedalus::DaedalusVM> createVm(const char16_t *datFile);

    static void debug(const ZenLoad::zCMesh &mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedMesh& mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedSkeletalMesh& mesh, std::ostream& out);

  private:
    std::u16string                     gpath;
    std::string                        wdef;
    bool                               noMenu=false;
    uint16_t                           pauseSum=0;

    std::thread                        loaderTh;
    std::atomic<LoadState>             loadingFlag{LoadState::Idle};

    std::unique_ptr<GameSession>       game;
    std::unique_ptr<FightAi>           fight;
    std::unique_ptr<CameraDefinitions> camera;
    std::unique_ptr<MusicDefinitions>  music;
  };
