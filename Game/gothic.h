#pragma once

#include <string>
#include <memory>

#include <Tempest/Signal>
#include <daedalus/DaedalusVM.h>

#include "world/world.h"

class Gothic final {
  public:
    Gothic(int argc,const char** argv);

    bool isInGame() const;
    bool doStartMenu() const { return !noMenu; }

    void setWorld(std::unique_ptr<World> &&w);
    const World& world() const { return *wrld; }
    World& world() { return *wrld; }

    void     pushPause();
    void     popPause();
    bool     isPause() const;

    uint64_t tickCount() const;
    void     tick(uint64_t dt);

    gtime    time() const { return  wrldTime; }
    void     setTime(gtime t);

    void     updateAnimation();

    auto     updateDialog(const WorldScript::DlgChoise &dlg) -> std::vector<WorldScript::DlgChoise>;
    void     dialogExec(const WorldScript::DlgChoise& dlg, Npc &player, Npc &hnpc);
    void     aiOuput(const char* msg);
    void     aiCloseDialog();
    void     printScreen(const char* msg, int x, int y, int time, const Tempest::Font &font);

    Tempest::Signal<void(const std::string&)> onSetWorld;
    Tempest::Signal<void(const char*)>        onDialogOutput;
    Tempest::Signal<void()>                   onDialogClose;

    Tempest::Signal<void(const char*,int,int,int,const Tempest::Font&)> onPrintScreen;

    const std::string&                    path() const { return gpath; }
    const std::string&                    defaultWorld() const;
    std::unique_ptr<Daedalus::DaedalusVM> createVm(const char* datFile);

    static void debug(const ZenLoad::zCMesh &mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedMesh& mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedSkeletalMesh& mesh, std::ostream& out);

  private:
    std::string wdef, gpath;
    bool        noMenu=false;
    uint16_t    pauseSum=0;

    uint64_t               ticks=0, wrldTimePart=0;
    gtime                  wrldTime;
    std::unique_ptr<World> wrld;

    static const uint64_t  multTime;
    static const uint64_t  divTime;
  };
