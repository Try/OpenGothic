#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Matrix4x4>
#include <string>
#include <functional>

#include <daedalus/DaedalusVM.h>
#include <zenload/zTypes.h>
#include <zenload/zTypes.h>

#include "graphics/worldview.h"
#include "graphics/lightgroup.h"
#include "graphics/meshobjects.h"
#include "physics/dynamicworld.h"
#include "triggers/trigger.h"
#include "game/gamescript.h"
#include "worldobjects.h"
#include "worldsound.h"
#include "waypoint.h"
#include "waymatrix.h"
#include "resources.h"

class GameSession;
class RendererStorage;
class Focus;
class WayMatrix;
class VisualFx;
class ParticleFx;
class Interactive;
class VersionInfo;

class World final {
  public:
    World()=delete;
    World(const World&)=delete;
    World(Gothic& gothic, GameSession &game, const RendererStorage& storage, std::string file, uint8_t isG2, std::function<void(int)> loadProgress);
    World(Gothic& gothic, GameSession &game, const RendererStorage& storage, Serialize& fin, uint8_t isG2, std::function<void(int)> loadProgress);

    struct BspSector final {
      int32_t guild=GIL_NONE;
      };

    void                 createPlayer(const char* cls);
    void                 insertPlayer(std::unique_ptr<Npc>&& npc, const char *waypoint);
    void                 postInit();
    const std::string&   name() const { return wname; }

    void                 load(Serialize& fin );
    void                 save(Serialize& fout);

    uint32_t             npcId(const Npc* ptr) const;
    Npc*                 npcById(uint32_t id);

    uint32_t             itmId(const void* ptr) const;
    Item*                itmById(uint32_t id);

    const WayPoint*      findPoint(const std::string& s, bool inexact=true) const { return findPoint(s.c_str(),inexact); }
    const WayPoint*      findPoint(const char* name, bool inexact=true) const;
    const WayPoint*      findWayPoint(const Tempest::Vec3& pos) const;
    const WayPoint*      findWayPoint(float x,float y,float z) const;

    const WayPoint*      findFreePoint(const Npc& pos,           const char* name) const;
    const WayPoint*      findFreePoint(const Tempest::Vec3& pos, const char* name) const;

    const WayPoint*      findNextFreePoint(const Npc& pos,const char* name) const;
    const WayPoint*      findNextPoint(const WayPoint& pos) const;

    void                 detectNpcNear(std::function<void(Npc&)> f);
    void                 detectNpc (const Tempest::Vec3& p, const float r, const std::function<void(Npc&)>& f);
    void                 detectItem(const Tempest::Vec3& p, const float r, const std::function<void(Item&)>& f);

    WayPath              wayTo(const Npc& pos,const WayPoint& end) const;
    WayPath              wayTo(float npcX,float npcY,float npcZ,const WayPoint& end) const;

    WorldView*           view()   const { return wview.get();    }
    DynamicWorld*        physic() const { return wdynamic.get(); }
    GameScript&          script() const;
    auto                 version() const -> const VersionInfo&;

    void                 assignRoomToGuild(const char* room, int32_t guildId);
    int32_t              guildOfRoom(const Tempest::Vec3& pos);
    int32_t              guildOfRoom(const char* portalName);

    void                 runEffect(Effect&& e);

    LightGroup::Light    getLight();
    LightGroup::Light    getLight(const ZenLoad::zCVobData& vob);
    MeshObjects::Mesh    getView(const Daedalus::ZString& visual) const;
    MeshObjects::Mesh    getView(const char*              visual) const;
    MeshObjects::Mesh    getView(const Daedalus::ZString& visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const;
    MeshObjects::Mesh    getView(const char*              visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const;
    PfxObjects::Emitter  getView(const ParticleFx* decl) const;
    PfxObjects::Emitter  getView(const ZenLoad::zCVobData& vob) const;
    MeshObjects::Mesh    getAtachView (const ProtoMesh::Attach& visual, const int32_t version);
    MeshObjects::Mesh    getItmView   (const Daedalus::ZString& visual, int32_t tex) const;
    MeshObjects::Mesh    getItmView   (const char*              visual, int32_t tex) const;
    MeshObjects::Mesh    getStaticView(const char* visual) const;
    MeshObjects::Mesh    getDecalView (const ZenLoad::zCVobData& vob) const;
    DynamicWorld::Item   getPhysic    (const char* visual);

    const VisualFx*      loadVisualFx(const char* name);
    const ParticleFx*    loadParticleFx(const char* name) const;

    void                 updateAnimation();
    void                 resetPositionToTA();

    auto                 takeHero() -> std::unique_ptr<Npc>;
    Npc*                 player() const { return npcPlayer; }
    Npc*                 findNpcByInstance(size_t instance);
    auto                 roomAt(const Tempest::Vec3& arr) -> const std::string&;

    void                 tick(uint64_t dt);
    uint64_t             tickCount() const;
    void                 setDayTime(int32_t h,int32_t min);
    gtime                time() const;

    Daedalus::PARSymbol& getSymbol(const char* s) const;
    size_t               getSymbolIndex(const char* s) const;

    Focus                validateFocus(const Focus& def);
    Focus                findFocus(const Npc& pl, const Focus &def);
    Focus                findFocus(const Focus& def);

    void                 triggerEvent(const TriggerEvent& e);
    void                 execTriggerEvent(const TriggerEvent& e);
    void                 enableTicks (AbstractTrigger& t);
    void                 disableTicks(AbstractTrigger& t);
    Interactive*         aviableMob(const Npc &pl, const char* name);
    Interactive*         findInteractive(const Npc& pl);
    void                 changeWorld(const std::string &world, const std::string &wayPoint);
    void                 setMobRoutine(gtime time, const Daedalus::ZString& scheme, int32_t state);

    void                 marchInteractives(Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;
    void                 marchPoints      (Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

    AiOuputPipe*         openDlgOuput(Npc &player, Npc &npc);

    void                 aiOutputSound(Npc &player, const std::string& msg);
    bool                 aiIsDlgFinished();

    bool                 isTargeted (Npc& npc);
    Npc*                 addNpc     (const char* name,    const Daedalus::ZString& at);
    Npc*                 addNpc     (size_t itemInstance, const Daedalus::ZString& at);
    Npc*                 addNpc     (size_t itemInstance, const Tempest::Vec3&     at);
    Item*                addItem    (size_t itemInstance, const char *at);
    Item*                addItem    (const ZenLoad::zCVobData& vob);
    Item*                takeItem   (Item& it);
    void                 removeItem (Item &it);
    size_t               hasItems(const char* tag, size_t itemCls);
    Bullet&              shootBullet(const Item &itmId, const Npc& npc, const Npc* target, const Interactive* inter);
    Bullet&              shootSpell(const Item &itm, const Npc &npc, const Npc *target);

    void                 sendPassivePerc (Npc& self,Npc& other,Npc& victum,int32_t perc);
    void                 sendPassivePerc (Npc& self,Npc& other,Npc& victum, Item& item,int32_t perc);
    void                 emitWeaponsSound(Npc& self,Npc& other);
    void                 emitLandHitSound(float x, float y, float z, uint8_t m0, uint8_t m1);
    void                 emitBlockSound  (Npc& self,Npc& other);

    bool                 isInListenerRange(const Tempest::Vec3& pos) const;
    void                 emitDlgSound   (const char *s, float x, float y, float z, float range, uint64_t &timeLen);
    void                 emitSoundEffect(const char *s, float x, float y, float z, float range, bool freeSlot);
    void                 emitSoundRaw   (const char *s, float x, float y, float z, float range, bool freeSlot);
    void                 emitSoundRaw3d (const char *s, float x, float y, float z, float range);
    void                 takeSoundSlot(GSoundEffect &&eff);
    void                 tickSlot(GSoundEffect &slot);

    void                 addTrigger    (AbstractTrigger* trigger);
    void                 addInteractive(Interactive* inter);
    void                 addStartPoint (const Tempest::Vec3& pos, const Tempest::Vec3& dir, const char* name);
    void                 addFreePoint  (const Tempest::Vec3& pos, const Tempest::Vec3& dir, const char* name);
    void                 addSound      (const ZenLoad::zCVobData& vob);

    void                 invalidateVobIndex();
    void                 triggerOnStart(bool firstTime);

  private:
    std::string                           wname;
    GameSession&                          game;

    std::unique_ptr<WayMatrix>            wmatrix;
    ZenLoad::zCBspTreeData                bsp;
    std::vector<BspSector>                bspSectors;

    Npc*                                  npcPlayer=nullptr;

    std::unique_ptr<DynamicWorld>         wdynamic;
    std::unique_ptr<WorldView>            wview;
    WorldSound                            wsound;
    WorldObjects                          wobj;
    std::unique_ptr<Npc>                  lvlInspector;

    auto         roomAt(const ZenLoad::zCBspNode &node) -> const std::string &;
    auto         portalAt(const std::string& tag) -> BspSector*;

    void         initScripts(bool firstTime);
  };
