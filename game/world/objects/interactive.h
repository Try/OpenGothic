#pragma once

#include <Tempest/Matrix4x4>
#include "physics/physicmesh.h"
#include "graphics/mesh/animationsolver.h"
#include "graphics/objvisual.h"
#include "game/inventory.h"
#include "utils/keycodec.h"
#include "vob.h"

#include <phoenix/vobs/mob.hh>

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

    Interactive(Vob* parent, World& world, const phoenix::vobs::mob& vob, Flags flags);

    void                load(Serialize& fin) override;
    void                save(Serialize& fout) const override;
    void                postValidate();

    void                resetPositionToTA(int32_t state);
    void                updateAnimation(uint64_t dt);
    void                tick(uint64_t dt);
    void                onKeyInput(KeyCodec::Action act);

    std::string_view    tag() const;
    std::string_view    focusName() const;
    bool                checkMobName(std::string_view dest) const;
    std::string_view    ownerName() const;

    bool                overrideFocus() const;

    Tempest::Vec3       displayPosition() const;
    std::string_view    displayName() const;

    auto                bBox() const -> const Tempest::Vec3*;

    int32_t             stateId() const { return state; }
    int32_t             stateCount() const { return stateNum; }
    bool                setMobState(std::string_view scheme,int32_t st) override;
    void                emitTriggerEvent() const;
    std::string_view    schemeName() const;
    std::string_view    posSchemeName() const;

    bool                isContainer() const;
    bool                isDoor() const;
    bool                isTrueDoor(const Npc& npc) const;
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
    bool                canQuitAtState(const Npc& npc, int32_t state) const;
    bool                attach (Npc& npc);
    bool                detach(Npc& npc,bool quick);
    bool                isAttached(const Npc& to);

    auto                animNpc(const AnimationSolver &solver, Anim t) -> const Animation::Sequence*;
    void                marchInteractives(DbgPainter& p) const;

  protected:
    Tempest::Matrix4x4  nodeTranform(std::string_view nodeName) const;
    void                moveEvent() override;
    float               extendedSearchRadius() const override;
    virtual void        onStateChanged(){}

  private:
    enum Phase : uint8_t {
      NonStarted = 0,
      Started    = 1,
      Quit       = 2,
      };

    struct Pos final {
      std::string         name;
      Npc*                user       = nullptr;
      Phase               started    = NonStarted;
      bool                attachMode = false;

      size_t              node=0;
      Tempest::Matrix4x4  pos;

      std::string_view    posTag() const;
      bool                isAttachPoint() const;
      bool                isDistPos() const;
      };

    void                setVisual(const phoenix::vob& vob);
    void                invokeStateFunc(Npc &npc);
    void                implTick(Pos &p);
    void                implQuitInteract(Pos &p);
    bool                setPos(Npc& npc, const Tempest::Vec3& pos);
    void                setDir(Npc& npc,const Tempest::Matrix4x4& mt);
    bool                attach(Npc& npc,Pos& to);
    void                implAddItem(std::string_view name);
    void                autoDetachNpc();
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

    std::string         vobName;
    std::string         focName;
    std::string         mdlVisual;
    Tempest::Vec3       bbox[2]={};
    std::string         owner;
    bool                focOver=false;
    bool                showVisual=true;
    Tempest::Vec3       displayOffset;
    // oCMobInter
    int                 stateNum=0;
    std::string         triggerTarget;
    std::string         useWithItem;
    std::string         conditionFunc;
    std::string         onStateFunc;
    bool                rewind = false;
    //  oCMobContainer
    bool                locked=false;
    std::string         keyInstance;
    std::string         pickLockStr;
    Inventory           invent;
    // oCMobLadder
    int                 stepsCount = 0;

    int32_t             state         = -1;
    bool                reverseState  = false;
    bool                loopState     = false;
    bool                isLockCracked = false;

    uint64_t            waitAnim      = 0;
    bool                animChanged   = false;

    std::vector<Pos>    attPos;
    PhysicMesh          physic;

    ObjVisual           visual;
  };
