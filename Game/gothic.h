#pragma once

#include <string>
#include <memory>
#include <thread>

#include <Tempest/Signal>
#include <daedalus/DaedalusVM.h>

#include "world/world.h"

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

    void         setWorld(std::unique_ptr<World> &&w);
    auto         clearWorld() -> std::unique_ptr<World>;
    const World* world() const { return wrld.get(); }
    World*       world() { return wrld.get(); }
    WorldView*   worldView() const;
    Npc*         player();

    void     pushPause();
    void     popPause();
    bool     isPause() const;

    LoadState checkLoading();
    bool      finishLoading();
    void      startLoading(const std::function<void()> f);
    void      cancelLoading();

    uint64_t tickCount() const;
    void     tick(uint64_t dt);

    gtime    time() const { return  wrldTime; }
    void     setTime(gtime t);

    void     updateAnimation();

    auto     updateDialog(const WorldScript::DlgChoise& dlg, Npc& player, Npc& npc) -> std::vector<WorldScript::DlgChoise>;
    void     dialogExec  (const WorldScript::DlgChoise& dlg, Npc& player, Npc& npc);

    void     aiProcessInfos (Npc& player, Npc& npc);
    bool     aiOuput(Npc& player, const char* msg);
    void     aiForwardOutput(Npc& player, const char* msg);
    bool     aiCloseDialog();
    bool     aiIsDlgFinished();

    void     printScreen(const char* msg, int x, int y, int time, const Tempest::Font &font);
    void     print      (const char* msg);

    Tempest::Signal<void(const std::string&)>              onSetWorld;
    Tempest::Signal<void(Npc&,Npc&)>                       onDialogProcess;
    Tempest::Signal<void(Npc&,const char*,bool&)>          onDialogOutput;
    Tempest::Signal<void(Npc&,const char*)>                onDialogForwardOutput;
    Tempest::Signal<void(bool&)>                           onDialogClose;
    Tempest::Signal<void(bool&)>                           isDialogClose;

    Tempest::Signal<void(const char*,int,int,int,const Tempest::Font&)> onPrintScreen;
    Tempest::Signal<void(const char*)>                                  onPrint;

    const std::string&                    messageByName(const std::string &id) const;
    uint32_t                              messageTime(const std::string &id) const;
    const std::u16string&                 path() const { return gpath; }
    const std::string&                    defaultWorld() const;
    std::unique_ptr<Daedalus::DaedalusVM> createVm(const char16_t *datFile);

    static void debug(const ZenLoad::zCMesh &mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedMesh& mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedSkeletalMesh& mesh, std::ostream& out);

  private:
    std::u16string gpath;
    std::string    wdef;
    bool           noMenu=false;
    uint16_t       pauseSum=0;

    std::thread            loaderTh;
    std::atomic<LoadState> loadingFlag{LoadState::Idle};

    uint64_t               ticks=0, wrldTimePart=0;
    gtime                  wrldTime;
    std::unique_ptr<World> wrld;

    static const uint64_t  multTime;
    static const uint64_t  divTime;
  };
