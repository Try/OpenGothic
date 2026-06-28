#pragma once

#include <Tempest/Matrix4x4>
#include <zenkit/vobs/MovableObject.hh>

#include "graphics/mesh/animationsolver.h"
#include "graphics/objvisual.h"
#include "world/triggers/abstracttrigger.h"
#include "game/inventory.h"
#include "utils/keycodec.h"
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

    Interactive(Vob* parent, World& world, const zenkit::VMovableObject& vob, Flags flags);

    void                load(Serialize& fin) override;
    void                save(Serialize& fout) const override;
    void                postValidate();

    void                drawVobBox(DbgPainter& p) const;
    void                drawVobRay(DbgPainter& p, const Npc& npc) const;

    void                resetPositionToTA(int32_t state);
    void                updateAnimation(uint64_t dt);
    void                tick(uint64_t dt);
    void                onKeyInput(KeyCodec::Action act);

    std::string_view    tag() const;
    std::string_view    focusName() const;
    bool                checkMobName(std::string_view dest) const;
    std::string_view    ownerName() const;
    std::string_view    ownerGuildName() const;

    bool                overrideFocus() const;

    Tempest::Vec3       displayPosition() const;
    std::string_view    displayName() const;

    auto                bBox() const -> const Tempest::Vec3*;

    int32_t             stateId() const { return state; }
    int32_t             stateCount() const { return stateNum; }
    bool                setMobState(std::string_view scheme,int32_t st) override;
    void                emitTriggerEvent(TriggerEvent::Type type) const;
    void                emitSoundEffect(std::string_view sound, float range, bool freeSlot);
    std::string_view    schemeName() const;
    std::string_view    posSchemeName() const;

    bool                isContainer() const;
    bool                isDoor() const;
    bool                isTrueDoor(const Npc& npc) const;
    bool                isLadder() const;
    std::string_view    pickLockCode() const { return pickLockStr; }
    void                setAsCracked(bool c) { isLockCracked = c; }
    bool                isCracked() const { return isLockCracked; }
    uint32_t            lockProgress() const          { return pickLockProgress; }
    void                setLockProgress(uint32_t p)   { pickLockProgress = p; }
    bool                needToLockpick(const Npc& pl) const;
    // NOTE: in original-game oCMobLockable::CanOpen (Gothic2.exe 0x007244f0) a locked container
    // opens only when the player owns the key, or owns a lockpick AND has the picklock talent;
    // else it refuses. Containers take the early inv.open path and never reach the mobsi attach
    // gate, so expose the existing key/lock condition for the open path (key-only chests must not
    // open without the key).
    bool                canOpen(Npc& npc) { return checkUseConditions(npc); }

    Inventory&          inventory();
    void                setSlotItem(MeshObjects::Mesh&& itm, std::string_view slot);

    uint32_t            stateMask() const;

    bool                canSeeNpc(const Npc &npc, bool freeLos) const;
    Tempest::Vec3       nearestPoint(const Npc& to) const;

    bool                isAvailable() const;
    void                reserveFor(Npc& npc);
    bool                isReservedForOther(const Npc& npc) const;
    bool                isStaticState() const;
    bool                isDetachState(const Npc& npc) const;
    bool                canQuitAtState(const Npc& npc, int32_t state) const;
    bool                attach(Npc& npc);
    bool                detach(Npc& npc, bool quick);
    bool                isAttached(const Npc& to);

    auto                animNpc(const AnimationSolver &solver, Anim t) const -> const Animation::Sequence*;
    void                marchInteractives(DbgPainter& p) const;

  protected:
    enum Phase : uint8_t {
      NotStarted = 0,
      Started    = 1,
      Quit       = 2,
      };

    struct Pos final {
      std::string         name;
      Npc*                user       = nullptr;
      Phase               started    = NotStarted;
      bool                attachMode = false;
      size_t              nodeId     = 0;

      Tempest::Matrix4x4  pos;

      std::string_view    posTag() const;
      bool                isAttachPoint() const;
      bool                isDistPos() const;
      };

    Tempest::Vec3       nodePosition(const Npc* npc, const Pos &to) const;
    Tempest::Matrix4x4  nodeTranform(const Npc* npc, const Pos &to) const;
    Tempest::Matrix4x4  mapBone(std::string_view nodeName) const;

    void                moveEvent() override;
    float               extendedSearchRadius() const override;
    virtual void        onStateChanged(){}

  private:
    void                setVisual(const zenkit::VirtualObject& vob);
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
    float               qDistTo(const Npc &npc, const Pos &to) const;

    std::string         vobName;
    std::string         focName;
    std::string         mdlVisual;
    Tempest::Vec3       bbox[2]={};
    std::string         owner;
    std::string         ownerGuild;
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
    // NOTE: in original-game oCMobLockable::PickLock (Gothic2.exe @0x00724800) the combination
    // index is stored on the mob (state dword @0x234 >> 2) and is reset to 0 ONLY on a wrong
    // keypress -- never on detach -- so partial progress persists per-lock and never bleeds
    // between different locks.
    uint32_t            pickLockProgress = 0;

    uint64_t            waitAnim      = 0;
    bool                animChanged   = false;

    std::vector<Pos>    attPos;
    ObjVisual           visual;

    // NOTE: in original-game oCMobInter::MarkAsUsed (Gothic2.exe @0x00720f20) the mob stores a
    // "reserved by NPC until time" pair (+0x22c/+0x230); EV_UseMob (@0x00754290) sets it to 20s
    // when an NPC starts walking toward the mob, and CanInteractWith/IsAvailable
    // (@0x00720f40/0x00720ec0) reject a *different* NPC (player exempt) while it is live. The
    // pointer is only ever compared (never dereferenced) and is re-validated in postValidate;
    // the original does not archive it, so it is intentionally not serialized.
    Npc*                reservedBy    = nullptr;
    uint64_t            reservedUntil = 0;
  };
