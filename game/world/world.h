#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Matrix4x4>
#include <string>
#include <functional>

#include <zenkit/World.hh>

#include "graphics/worldview.h"
#include "graphics/lightgroup.h"
#include "graphics/meshobjects.h"
#include "game/gamescript.h"
#include "physics/dynamicworld.h"
#include "worldobjects.h"
#include "worldsound.h"
#include "waypoint.h"
#include "waymatrix.h"

class GameSession;
class Focus;
class WayMatrix;
class VisualFx;
class GlobalEffects;
class ParticleFx;
class Interactive;
class VersionInfo;
class GlobalFx;

class World final {
  public:
    World()=delete;
    World(const World&)=delete;
    World(GameSession& game, std::string_view file, bool startup, std::function<void(int)> loadProgress);
    ~World();

    void                 createPlayer(std::string_view cls);
    void                 insertPlayer(std::unique_ptr<Npc>&& npc, std::string_view waypoint);
    void                 setPlayer(Npc* npc);
    void                 postInit();
    std::string_view     name() const { return wname; }

    void                 load(Serialize& fin );
    void                 save(Serialize& fout);

    uint32_t             npcId(const Npc* ptr) const;
    Npc*                 npcById(uint32_t id);
    uint32_t             npcCount() const;

    uint32_t             mobsiId(const Interactive* ptr) const;
    Interactive*         mobsiById(uint32_t id);

    uint32_t             itmId(const void* ptr) const;
    Item*                itmById(uint32_t id);

    const WayPoint*      findPoint(std::string_view name, bool inexact=true) const;
    const WayPoint*      findWayPoint(const Tempest::Vec3& pos) const;
    const WayPoint*      findWayPoint(const Tempest::Vec3& pos, const std::function<bool(const WayPoint&)>& f) const;

    const WayPoint*      findFreePoint(const Npc& pos,           std::string_view name) const;
    const WayPoint*      findFreePoint(const Tempest::Vec3& pos, std::string_view name) const;

    const WayPoint*      findNextFreePoint(const Npc& pos, std::string_view name) const;
    const WayPoint*      findNextWayPoint(const Npc &npc) const;
    const WayPoint*      findNextPoint(const WayPoint& pos) const;

    const WayPoint&      startPoint() const;
    const WayPoint&      deadPoint() const;

    void                 detectNpcNear(std::function<void(Npc&)> f);
    void                 detectNpc (const Tempest::Vec3& p, const float r, const std::function<void(Npc&)>& f);
    void                 detectItem(const Tempest::Vec3& p, const float r, const std::function<void(Item&)>& f);

    WayPath              wayTo(const Npc& pos,const WayPoint& end) const;

    WorldView*           view()     const { return wview.get();    }
    WorldSound*          sound()          { return &wsound;        }
    DynamicWorld*        physic()   const { return wdynamic.get(); }
    GlobalEffects*       globalFx() const { return globFx.get();   }

    GameScript&          script()   const;
    GameSession&         gameSession() const { return game; }
    auto                 version()  const -> const VersionInfo&;

    void                 assignRoomToGuild(std::string_view room, int32_t guildId);
    int32_t              guildOfRoom(const Tempest::Vec3& pos);
    int32_t              guildOfRoom(std::string_view portalName);

    void                 runEffect(Effect&& e);
    void                 stopEffect(const VisualFx& vfx);

    GlobalFx             addGlobalEffect(std::string_view what, uint64_t len, const std::string* argv, size_t argc);
    MeshObjects::Mesh    addView(std::string_view visual) const;
    MeshObjects::Mesh    addView(std::string_view visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const;
    MeshObjects::Mesh    addView(const zenkit::IItem& itm);
    MeshObjects::Mesh    addView(const ProtoMesh* visual);
    MeshObjects::Mesh    addAtachView (const ProtoMesh::Attach& visual, const int32_t version);
    MeshObjects::Mesh    addStaticView(const ProtoMesh* visual, bool staticDraw);
    MeshObjects::Mesh    addStaticView(std::string_view visual);
    MeshObjects::Mesh    addDecalView (const zenkit::VisualDecal& decal);
    LightGroup::Light    addLight(const zenkit::VLight& vob);
    LightGroup::Light    addLight(std::string_view preset);

    void                 updateAnimation(uint64_t dt);
    void                 resetPositionToTA();

    auto                 takeHero() -> std::unique_ptr<Npc>;
    Npc*                 player() const { return npcPlayer; }
    Npc*                 findNpcByInstance(size_t instance, size_t n = 0);
    Item*                findItemByInstance(size_t instance, size_t n = 0);
    std::string_view     roomAt(const Tempest::Vec3& arr);

    void                 scaleTime(uint64_t& dt);
    void                 tick(uint64_t dt);
    uint64_t             tickCount() const;
    void                 setDayTime(int32_t h,int32_t min);
    gtime                time() const;

    Focus                validateFocus(const Focus& def);
    Focus                findFocus(const Npc& pl, const Focus &def);
    Focus                findFocus(const Focus& def);
    bool                 testFocusNpc(Npc *def);

    void                 triggerOnStart(bool firstTime);
    void                 triggerEvent(const TriggerEvent& e);
    void                 triggerChangeWorld(std::string_view world, std::string_view wayPoint);
    void                 execTriggerEvent(const TriggerEvent& e);
    void                 enableDefTrigger(AbstractTrigger& t);
    void                 enableTicks (AbstractTrigger& t);
    void                 disableTicks(AbstractTrigger& t);
    void                 setCurrentCs(CsCamera* cs);
    CsCamera*            currentCs() const;
    bool                 isCutsceneLock() const;
    void                 enableCollizionZone (CollisionZone& z);
    void                 disableCollizionZone(CollisionZone& z);

    Interactive*         availableMob(const Npc &pl, std::string_view name);
    Interactive*         findInteractive(const Npc& pl);
    void                 setMobRoutine(gtime time, std::string_view scheme, int32_t state);

    void                 marchInteractives(DbgPainter& p) const;
    void                 marchPoints      (DbgPainter& p) const;

    AiOuputPipe*         openDlgOuput(Npc &player, Npc &npc);
    void                 aiOutputSound(Npc &player, std::string_view msg);
    bool                 aiIsDlgFinished();

    bool                 isTargeted (Npc& npc);
    Npc*                 addNpc     (std::string_view name, std::string_view     at);
    Npc*                 addNpc     (size_t npcInstance,    std::string_view     at);
    Npc*                 addNpc     (size_t npcInstance,    const Tempest::Vec3& at);
    void                 removeNpc  (Npc& npc);

    Item*                addItem    (size_t itemInstance,   std::string_view     at);
    Item*                addItem    (const zenkit::VItem& vob);
    Item*                addItem    (size_t itemInstance, const Tempest::Vec3&      pos);
    Item*                addItemDyn (size_t itemInstance, const Tempest::Matrix4x4& pos, size_t owner);
    auto                 takeItem   (Item& it) -> std::unique_ptr<Item>;
    void                 removeItem (Item& it);
    size_t               hasItems(std::string_view tag, size_t itemCls);

    Bullet&              shootBullet(const Item &itmId, const Npc& npc, const Npc* target, const Interactive* inter);
    Bullet&              shootSpell(const Item &itm, const Npc &npc, const Npc *target);

    void                 sendPassivePerc  (Npc& self, Npc& other, Item& item, int32_t perc);
    void                 sendPassivePerc  (Npc& self, Npc& other, Npc& victum, int32_t perc);
    void                 sendPassivePerc  (Npc& self, Npc& other, Npc& victum, Item& item, int32_t perc);

    void                 sendImmediatePerc(Npc& self, Npc& other, Npc& victum, int32_t perc);
    void                 sendImmediatePerc(Npc& self, Npc& other, Npc& victum, Item& item, int32_t perc);

    bool                 isInSfxRange(const Tempest::Vec3& pos) const;
    bool                 isInPfxRange(const Tempest::Vec3& pos) const;

    void                 addDlgSound(std::string_view s, const Tempest::Vec3& pos, float range, uint64_t &timeLen);

    Sound                addWeaponHitEffect(Npc&         src, const Bullet* srcArrow, Npc&  reciver);
    Sound                addWeaponBlkEffect(ItemMaterial src, ItemMaterial          reciver, const Tempest::Matrix4x4& pos);
    Sound                addLandHitEffect  (ItemMaterial src, zenkit::MaterialGroup reciver, const Tempest::Matrix4x4& pos);

    void                 addTrigger    (AbstractTrigger* trigger);
    void                 addInteractive(Interactive* inter);
    void                 addStartPoint (const Tempest::Vec3& pos, const Tempest::Vec3& dir, std::string_view name);
    void                 addFreePoint  (const Tempest::Vec3& pos, const Tempest::Vec3& dir, std::string_view name);
    void                 addSound      (const zenkit::VirtualObject& vob);

    void                 invalidateVobIndex();

  private:
    const zenkit::IFocus& searchPolicy(const Npc& pl, TargetCollect& collAlgo, TargetType& collType, WorldObjects::SearchFlg& opt) const;
    std::string                           wname;
    GameSession&                          game;

    std::unique_ptr<WayMatrix>            wmatrix;

    struct BspSector final {
      int32_t guild=GIL_NONE;
      };

    struct {
      std::vector<zenkit::BspNode>        nodes;
      std::vector<zenkit::BspSector>      sectors;
      std::vector<std::uint64_t>          leaf_node_indices;
      std::vector<BspSector>              sectorsData;
      } bsp;

    Npc*                                  npcPlayer=nullptr;

    std::unique_ptr<DynamicWorld>         wdynamic;
    std::unique_ptr<WorldView>            wview;
    std::unique_ptr<GlobalEffects>        globFx;
    WorldSound                            wsound;
    WorldObjects                          wobj;
    std::unique_ptr<Npc>                  lvlInspector;

    auto         roomAt(const zenkit::BspNode &node) -> std::string_view;
    auto         portalAt(std::string_view tag) -> BspSector*;

    void         initScripts(bool firstTime);

    Sound        addHitEffect(std::string_view src, std::string_view reciver, std::string_view scheme, const Tempest::Matrix4x4& pos);
  };
