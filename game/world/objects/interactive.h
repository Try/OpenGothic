#pragma once

#include <Tempest/Matrix4x4>
#include "physics/physicmesh.h"
#include "graphics/mesh/animationsolver.h"
#include "graphics/mesh/protomesh.h"
#include "graphics/objvisual.h"
#include "graphics/meshobjects.h"
#include "game/inventory.h"
#include "vob.h"

class Npc;
class World;
class Trigger;

class Interactive : public Vob {
  public:
    enum Anim : int8_t {
      In        =  1,
      Active    =  0,
      Out       = -1,

      ToStand   = 10,
      FromStand = 11,
      };

    Interactive(Vob* parent, World& world, ZenLoad::zCVobData& vob, Flags flags);

    void                load(Serialize& fin) override;
    void                save(Serialize& fout) const override;
    void                postValidate();

    void                resetPositionToTA(int32_t state);
    void                updateAnimation(uint64_t dt);
    void                tick(uint64_t dt);

    std::string_view    tag() const;
    std::string_view    focusName() const;
    bool                checkMobName(std::string_view dest) const;
    std::string_view    ownerName() const;

    bool                overrideFocus() const;

    Tempest::Vec3       displayPosition() const;
    std::string_view    displayName() const;

    int32_t             stateId() const { return state; }
    int32_t             stateCount() const { return stateNum; }
    bool                setMobState(std::string_view scheme,int32_t st) override;
    void                emitTriggerEvent() const;
    std::string_view    schemeName() const;
    std::string_view    posSchemeName() const;

    bool                isContainer() const;
    bool                isLadder() const;
    std::string_view    pickLockCode() const { return pickLockStr; }
    void                setAsCracked(bool c) { isLockCracked = c; }
    bool                isCracked() const { return isLockCracked; }
    bool                needToLockpick(const Npc& pl) const;

    Inventory&          inventory();

    uint32_t            stateMask() const;

    bool                canSeeNpc(const Npc &npc, bool freeLos) const;
    Tempest::Vec3       nearestPoint(const Npc& to) const;

    bool                isAvailable() const;
    bool                isStaticState() const;
    bool                isDetachState(const Npc& npc) const;
    bool                canQuitAtState(Npc& npc, int32_t state) const;
    bool                attach (Npc& npc);
    bool                dettach(Npc& npc,bool quick);
    bool                isAttached(const Npc& to);

    auto                animNpc(const AnimationSolver &solver, Anim t) -> const Animation::Sequence*;
    void                marchInteractives(DbgPainter& p) const;

    void                nextState(Npc& owner);

  protected:
    Tempest::Matrix4x4  nodeTranform(std::string_view nodeName) const;
    void                moveEvent() override;
    float               extendedSearchRadius() const override;
    virtual void        onStateChanged(){}

  private:
    struct Pos final {
      std::string        name;
      Npc*               user=nullptr;
      bool               started=false;
      bool               attachMode=false;

      size_t             node=0;
      Tempest::Matrix4x4 pos;

      std::string_view   posTag() const;
      bool               isAttachPoint() const;
      bool               isDistPos() const;
      };

    void                setVisual(ZenLoad::zCVobData& vob);
    void                invokeStateFunc(Npc &npc);
    void                implTick(Pos &p, uint64_t dt);
    void                implQuitInteract(Pos &p);
    void                setPos(Npc& npc, const Tempest::Vec3& pos);
    void                setDir(Npc& npc,const Tempest::Matrix4x4& mt);
    bool                attach(Npc& npc,Pos& to);
    void                implAddItem(std::string_view name);
    void                autoDettachNpc();
    void                implChState(bool next);
    bool                checkUseConditions(Npc& npc);

    auto                setAnim(Anim t) -> const Animation::Sequence*;
    bool                setAnim(Npc* npc, Anim dir);
    void                setState(int st);

    template<class P, class Inter>
    static P*           findNearest(Inter& in, const Npc& to);

    const Pos*          findNearest(const Npc& to) const;
    Pos*                findNearest(const Npc& to);
    const Pos*          findFreePos() const;
    Pos*                findFreePos();
    auto                worldPos(const Pos &to) const -> Tempest::Vec3;
    float               qDistanceTo(const Npc &npc, const Pos &to) const;
    Tempest::Matrix4x4  nodeTranform(const Npc &npc, const Pos &p) const;
    auto                nodePosition(const Npc &npc, const Pos &p) const -> Tempest::Vec3;

    std::string                  vobName;
    std::string                  focName;
    std::string                  mdlVisual;
    Tempest::Vec3                bbox[2]={};
    std::string                  owner;
    bool                         focOver=false;
    bool                         showVisual=true;
    Tempest::Vec3                displayOffset;
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
    // oCMobLadder
    int                          stepsCount = 0;

    int32_t                      state         = -1;
    bool                         reverseState  = false;
    bool                         loopState     = false;
    bool                         isLockCracked = false;

    uint64_t                     waitAnim      = 0;
    bool                         animChanged   = false;

    std::vector<Pos>             attPos;
    PhysicMesh                   physic;

    ObjVisual                    visual;
  };
