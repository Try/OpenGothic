#pragma once

#include <Tempest/Matrix4x4>
#include "physics/dynamicworld.h"
#include "graphics/animationsolver.h"
#include "graphics/staticobjects.h"
#include "graphics/protomesh.h"
#include "game/inventory.h"

class Npc;
class World;
class Trigger;

class Interactive final {
  public:
    enum Anim : int8_t {
      In    = 1,
      Active= 0,
      Out   =-1
      };

    Interactive(World& owner,const ZenLoad::zCVobData &vob);
    Interactive(Interactive&&)=default;

    const std::string&  tag() const;
    const std::string&  focusName() const;
    bool                checkMobName(const std::string& dest) const;

    std::array<float,3> position() const;
    std::array<float,3> displayPosition() const;
    const char*         displayName() const;

    std::string         stateFunc() const;
    int32_t             stateId() const { return state; }
    Trigger*            triggerTarget() const;

    bool                isContainer() const;
    Inventory&          inventory();

    uint32_t            stateMask(uint32_t orig) const;

    bool isAvailable() const;
    bool attach (Npc& npc);
    void dettach(Npc& npc);

    void nextState();
    void prevState();
    auto anim(const AnimationSolver &solver, Anim t) const -> AnimationSolver::Sequence;
    void marchInteractives(Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

    Tempest::Matrix4x4     objMat;

  private:
    struct Pos final {
      Npc*               user=nullptr;
      std::string        name;
      size_t             node=0;
      Tempest::Matrix4x4 pos;
      bool               isAttachPoint() const;
      };

    void setPos(Npc& npc,std::array<float,3> pos);
    void setDir(Npc& npc,const Tempest::Matrix4x4& mt);
    void attach(Npc& npc,Pos& to);
    void implAddItem(char *name);

    const Pos* findFreePos() const;
    Pos*       findFreePos();
    auto       worldPos(const Pos &to) const -> std::array<float,3>;
    float      qDistanceTo(const Npc &npc, const Pos &to);

    World*             world = nullptr;
    ZenLoad::zCVobData data;
    Inventory          invent;
    int                state=0;

    std::vector<Pos>         pos;
    const ProtoMesh*         mesh = nullptr;
    StaticObjects::Mesh      view;
    DynamicWorld::StaticItem physic;
  };
