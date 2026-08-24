#pragma once

#include <Tempest/Sound>
#include <Tempest/SoundDevice>
#include <memory>

#include "game/gamescript.h"
#include "camera.h"
#include "gametime.h"

class World;
class WorldView;
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
    GameSession(std::string file);
    GameSession(Serialize&  fin);
    ~GameSession();

    void         save(Serialize& fout, std::string_view name, const Tempest::Pixmap &screen);
    void         setupSettings();

    void         setWorld(std::unique_ptr<World> &&w);
    auto         clearWorld() -> std::unique_ptr<World>;

    void         changeWorld(std::string_view world, std::string_view wayPoint);
    void         exitSession();

    auto         version() const -> const VersionInfo&;

    const World* world() const { return wrld.get(); }
    World*       world()       { return wrld.get(); }

    WorldView*   view()   const;
    GameScript*  script() const { return vm.get(); }

    Camera&      camera()       { return     *cam; }

    auto         loadSound(const Tempest::Sound& raw) -> Tempest::SoundEffect;
    auto         loadSound(const SoundFx&        fx, bool& looped)  -> Tempest::SoundEffect;

    Npc*         player();
    void         updateListenerPos(const Camera::ListenerPos& lpos);

    gtime        time() const { return  wrldTime; }
    void         setTime(gtime t);
    void         tick(uint64_t dt);
    uint64_t     tickCount() const { return ticks; }

    void         setTimeMultiplyer(float t);

    void         updateAnimation(uint64_t dt);

    auto         updateDialog(const GameScript::DlgChoice &dlg, Npc &player, Npc &npc) -> std::vector<GameScript::DlgChoice>;
    void         dialogExec(const GameScript::DlgChoice &dlg, Npc &player, Npc &npc);

    std::string_view         messageFromSvm(std::string_view id, int voice) const;
    std::string_view         messageByName (std::string_view id) const;
    uint32_t                 messageTime   (std::string_view id) const;

    AiOuputPipe* openDlgOuput(Npc &player, Npc &npc);
    bool         isNpcInDialog(const Npc& npc) const;
    bool         isInDialog() const;

  private:
    struct ChWorld {
      std::string zen, wp;
      };

    struct HeroStorage {
      void                 save(Npc& npc);
      void                 putToWorld(World &owner, std::string_view wayPoint) const;

      std::vector<uint8_t> storage;
      };

    bool         isWorldKnown(std::string_view name) const;
    void         initPerceptions();
    void         initScripts(bool firstTime);
    auto         implChangeWorld(std::unique_ptr<GameSession> &&game, std::string_view world, std::string_view wayPoint) -> std::unique_ptr<GameSession>;
    auto         findStorage(std::string_view name) -> const WorldStateStorage&;

    Tempest::SoundDevice           sound;

    std::unique_ptr<Camera>        cam;
    std::unique_ptr<GameScript>    vm;
    std::unique_ptr<World>         wrld;

    uint64_t                       ticks = 0, wrldTimePart = 0;
    uint64_t                       timeMul = 1000, timeMulFract = 0;
    gtime                          wrldTime;

    std::vector<WorldStateStorage> visitedWorlds;

    ChWorld                        chWorld;
    bool                           exitSessionFlg=false;

    static const uint64_t          multTime;
    static const uint64_t          divTime;
  };
