#pragma once

#include <Tempest/Matrix4x4>
#include "physics/dynamicworld.h"
#include "graphics/animationsolver.h"
#include "graphics/meshobjects.h"
#include "graphics/protomesh.h"
#include "game/inventory.h"

class Npc;
class World;
class Trigger;

class Interactive final {
  public:
    enum Anim : int8_t {
      In        =  1,
      Active    =  0,
      Out       = -1,

      ToStand   = 10,
      FromStand = 11,
      };

    Interactive(World& owner, ZenLoad::zCVobData &&vob);
    Interactive(World &world);
    Interactive(Interactive&&)=default;

    void                load(Serialize& fin);
    void                save(Serialize& fout) const;

    void                updateAnimation();
    void                tick(uint64_t dt);

    const std::string&  tag() const;
    const std::string&  focusName() const;
    bool                checkMobName(const char* dest) const;
    const std::string&  ownerName() const;

    bool                overrideFocus() const;

    std::array<float,3> position() const;
    std::array<float,3> displayPosition() const;
    const char*         displayName() const;
    auto                transform() const -> const Tempest::Matrix4x4& { return pos; }

    int32_t             stateId() const { return state; }
    void                emitTriggerEvent() const;
    const char*         schemeName() const;

    bool                isContainer() const;
    Inventory&          inventory();

    uint32_t            stateMask() const;

    bool                canSeeNpc(const Npc &npc, bool freeLos) const;

    bool                isAvailable() const;
    bool                isStaticState() const;
    bool                canQuitAtLastState() const;
    bool                attach (Npc& npc);
    bool                dettach(Npc& npc,bool quick);

    auto                animNpc(const AnimationSolver &solver, Anim t) -> const Animation::Sequence*;
    void                marchInteractives(Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

  private:
    struct Pos final {
      std::string        name;
      Npc*               user=nullptr;
      bool               started=false;
      bool               attachMode=false;

      size_t             node=0;
      Tempest::Matrix4x4 pos;

      const char*        posTag() const;
      bool               isAttachPoint() const;
      bool               isDistPos() const;
      };

    void                setVisual(const std::string& visual);
    void                invokeStateFunc(Npc &npc);
    void                implTick(Pos &p, uint64_t dt);
    void                implQuitInteract(Pos &p);
    void                setPos(Npc& npc,std::array<float,3> pos);
    void                setDir(Npc& npc,const Tempest::Matrix4x4& mt);
    bool                attach(Npc& npc,Pos& to);
    void                implAddItem(char *name);
    void                autoDettachNpc();
    void                implChState(bool next);
    bool                checkUseConditions(Npc& npc);

    auto                setAnim(Anim t) -> const Animation::Sequence*;
    bool                setAnim(Npc* npc, Anim dir);

    const Pos*          findFreePos() const;
    Pos*                findFreePos();
    auto                worldPos(const Pos &to) const -> std::array<float,3>;
    float               qDistanceTo(const Npc &npc, const Pos &to);
    Tempest::Matrix4x4  nodeTranform(const Npc &npc, const Pos &p) const;
    auto                nodePosition(const Npc &npc, const Pos &p) const -> std::array<float,3>;

    World*                       world = nullptr;

    ZenLoad::zCVobData::EVobType vobType=ZenLoad::zCVobData::EVobType::VT_oCMOB;
    std::string                  vobName;
    std::string                  focName;
    std::string                  mdlVisual;
    ZMath::float3                bbox[2]={};
    std::string                  owner;
    bool                         focOver=false;
    bool                         showVisual=true;
    // oCMobInter
    int                          stateNum=0;
    std::string                  triggerTarget;
    std::string                  useWithItem;
    std::string                  conditionFunc;
    std::string                  onStateFunc;
    bool                         rewind = false;
    //  oCMobContainer
    bool                         locked=false;
    std::string                  keyInstance;
    std::string                  pickLockStr;
    Inventory                    invent;

    Tempest::Matrix4x4           pos;
    int                          state=0;
    bool                         reverseState=false;
    bool                         loopState=false;
    uint64_t                     waitAnim = 0;

    std::vector<Pos>             attPos;
    const ProtoMesh*             mesh = nullptr;
    DynamicWorld::StaticItem     physic;

    const Skeleton*              skeleton=nullptr;
    MeshObjects::Mesh            view;
    AnimationSolver              solver;
    std::unique_ptr<Pose>        skInst;
  };
