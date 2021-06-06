#pragma once

#include <Tempest/Sound>
#include <Tempest/SoundDevice>
#include <memory>

#include "ui/documentmenu.h"
#include "ui/chapterscreen.h"
#include "game/gamescript.h"
#include "gamemusic.h"
#include "gametime.h"

class Camera;
class World;
class WorldView;
class RendererStorage;
class Npc;
class Serialize;
class GSoundEffect;
class SoundFx;
class ParticleFx;
class VisualFx;
class WorldStateStorage;
class VersionInfo;
class GthFont;

class GameSession final {
  public:
    GameSession()=delete;
    GameSession(const GameSession&)=delete;
    GameSession(const RendererStorage& storage, std::string file);
    GameSession(const RendererStorage& storage, Serialize&  fin);
    ~GameSession();

    void         save(Serialize& fout, const char *name, const Tempest::Pixmap &screen);

    void         setWorld(std::unique_ptr<World> &&w);
    auto         clearWorld() -> std::unique_ptr<World>;

    void         changeWorld(const std::string &world, const std::string &wayPoint);
    void         exitSession();

    auto         version() const -> const VersionInfo&;

    const World* world() const { return wrld.get(); }
    World*       world()       { return wrld.get(); }

    WorldView*   view()   const;
    GameScript*  script() const { return vm.get(); }

    Camera&      camera()       { return     *cam; }

    auto         loadScriptCode() -> std::vector<uint8_t>;

    SoundFx*     loadSoundFx    (const char *name);
    SoundFx*     loadSoundWavFx (const char *name);
    auto         loadParticleFx (const char* name) -> const ParticleFx*;
    auto         loadParticleFx (const Daedalus::GEngineClasses::C_ParticleFXEmitKey& k) -> const ParticleFx*;
    auto         loadVisualFx   (const char* name) -> const VisualFx*;
    auto         loadSound      (const Tempest::Sound& raw) -> Tempest::SoundEffect;
    auto         loadSound      (const SoundFx&        fx, bool& looped)  -> Tempest::SoundEffect;
    void         emitGlobalSound(const Tempest::Sound& sfx);
    void         emitGlobalSound(const std::string& sfx);

    Npc*         player();
    void         updateListenerPos(Npc& npc);

    gtime        time() const { return  wrldTime; }
    void         setTime(gtime t);
    void         tick(uint64_t dt);
    uint64_t     tickCount() const { return ticks; }

    void         updateAnimation();

    auto         updateDialog(const GameScript::DlgChoise &dlg, Npc &player, Npc &npc) -> std::vector<GameScript::DlgChoise>;
    void         dialogExec(const GameScript::DlgChoise &dlg, Npc &player, Npc &npc);

    const Daedalus::ZString& messageFromSvm(const Daedalus::ZString& id, int voice) const;
    const Daedalus::ZString& messageByName (const Daedalus::ZString& id) const;
    uint32_t                 messageTime   (const Daedalus::ZString& id) const;

    AiOuputPipe* openDlgOuput(Npc &player, Npc &npc);
    bool         aiIsDlgFinished();

  private:
    struct ChWorld {
      std::string zen, wp;
      };

    struct HeroStorage {
      void                 save(Npc& npc, World &owner);
      void                 putToWorld(World &owner, const std::string &wayPoint) const;

      std::vector<uint8_t> storage;
      };

    bool         isWorldKnown(const std::string& name) const;
    void         initScripts(bool firstTime);
    auto         implChangeWorld(std::unique_ptr<GameSession> &&game, const std::string &world, const std::string &wayPoint) -> std::unique_ptr<GameSession>;
    auto         findStorage(const std::string& name) -> const WorldStateStorage&;

    const RendererStorage&         storage;
    Tempest::SoundDevice           sound;

    std::unique_ptr<Camera>        cam;
    std::unique_ptr<GameScript>    vm;
    std::unique_ptr<World>         wrld;

    uint64_t                       ticks=0, wrldTimePart=0;
    gtime                          wrldTime;

    std::vector<WorldStateStorage> visitedWorlds;

    ChWorld                        chWorld;
    bool                           exitSessionFlg=false;

    static const uint64_t          multTime;
    static const uint64_t          divTime;
  };
