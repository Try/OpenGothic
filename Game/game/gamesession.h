#pragma once

#include <Tempest/Sound>
#include <Tempest/SoundDevice>
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
class GSoundEffect;
class SoundFx;
class WorldStateStorage;

class GameSession final {
  public:
    GameSession()=delete;
    GameSession(const GameSession&)=delete;
    GameSession(Gothic &gothic, const RendererStorage& storage, std::string file);
    GameSession(Gothic &gothic, const RendererStorage& storage, Serialize&  fin);
    ~GameSession();

    void         save(Serialize& fout);

    void         setWorld(std::unique_ptr<World> &&w);
    auto         clearWorld() -> std::unique_ptr<World>;

    void         changeWorld(const std::string &world, const std::string &wayPoint);

    bool         isRamboMode() const;

    const World* world() const { return wrld.get(); }
    World*       world()       { return wrld.get(); }

    WorldView*   view()   const;
    WorldScript* script() const { return vm.get(); }

    auto         loadScriptCode() -> std::vector<uint8_t>;
    SoundFx*     loadSoundFx(const char *name);
    auto         loadSound(const Tempest::Sound& raw) -> Tempest::SoundEffect;
    auto         loadSound(const SoundFx&        fx)  -> GSoundEffect;
    void         emitGlobalSound(const Tempest::Sound& sfx);
    void         emitGlobalSound(const std::string& sfx);

    Npc*         player();
    void         updateListenerPos(Npc& npc);

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

  private:
    struct ChWorld {
      std::string zen, wp;
      };

    struct HeroStorage {
      void                 save(Npc& npc);
      std::unique_ptr<Npc> load(World &owner) const;

      std::vector<uint8_t> storage;
      };

    bool         isWorldKnown(const std::string& name) const;
    void         initScripts(bool firstTime);
    void         implChangeWorld(const HeroStorage &hero, const std::string &world, const std::string &wayPoint);
    auto         allocWorld(const std::string& name, std::function<void(int)> loadProgress) -> std::unique_ptr<World>;

    Gothic&                        gothic;
    const RendererStorage&         storage;

    Tempest::SoundDevice           sound;
    std::unique_ptr<WorldScript>   vm;
    std::unique_ptr<World>         wrld;

    uint64_t                       ticks=0, wrldTimePart=0;
    gtime                          wrldTime;

    std::vector<WorldStateStorage> visitedWorlds;

    ChWorld                        chWorld;
    ChapterScreen::Show            chapter;
    bool                           pendingChapter=false;

    static const uint64_t          multTime;
    static const uint64_t          divTime;
  };
