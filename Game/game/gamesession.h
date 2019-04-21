#pragma once

#include <memory>
#include "ui/chapterscreen.h"
#include "world/worldscript.h"
#include "gametime.h"

class Gothic;
class World;
class WorldView;
class RendererStorage;
class Npc;
class Serialize;

class GameSession final {
  public:
    GameSession()=delete;
    GameSession(const World&)=delete;
    GameSession(Gothic &gothic, const RendererStorage& storage, std::string file, std::function<void(int)> loadProgress);
    GameSession(Gothic &gothic, const RendererStorage& storage, Serialize&  fin, std::function<void(int)> loadProgress);
    ~GameSession();

    void         save(Serialize& fout);

    void         setWorld(std::unique_ptr<World> &&w);
    auto         clearWorld() -> std::unique_ptr<World>;

    const World* world() const { return wrld.get(); }
    World*       world()       { return wrld.get(); }

    WorldView*   view()   const;
    WorldScript* script() const { return vm.get(); }

    auto         loadScriptCode() -> std::vector<uint8_t>;

    Npc*         player();

    gtime        time() const { return  wrldTime; }
    void         setTime(gtime t);
    void         tick(uint64_t dt);
    uint64_t     tickCount() const { return ticks; }

    void         updateAnimation();

    auto         updateDialog(const WorldScript::DlgChoise &dlg, Npc &player, Npc &npc) -> std::vector<WorldScript::DlgChoise>;
    void         dialogExec(const WorldScript::DlgChoise &dlg, Npc &player, Npc &npc);

    const std::string &messageByName(const std::string &id) const;
    uint32_t     messageTime(const std::string &id) const;

    void         aiProcessInfos(Npc &player, Npc &npc);
    bool         aiOutput(Npc &player, const char *msg);
    void         aiForwardOutput(Npc &player, const char *msg);
    bool         aiCloseDialog();
    bool         aiIsDlgFinished();
    void         printScreen(const char *msg, int x, int y, int time, const Tempest::Font &font);
    void         print(const char *msg);
    void         introChapter(const ChapterScreen::Show& s);

    auto         getFightAi(size_t i) const -> const FightAi::FA&;
    auto         getMusicTheme(const char* name) const ->  const Daedalus::GEngineClasses::C_MusicTheme&;
    auto         getSoundScheme(const char* name) const ->  const Daedalus::GEngineClasses::C_SFX&;

  private:
    void         initScripts(bool firstTime);

    Gothic&                      gothic;

    std::unique_ptr<WorldScript> vm;
    std::unique_ptr<World>       wrld;

    uint64_t                     ticks=0, wrldTimePart=0;
    gtime                        wrldTime;

    ChapterScreen::Show          chapter;
    bool                         pendingChapter=false;

    static const uint64_t        multTime;
    static const uint64_t        divTime;
  };
