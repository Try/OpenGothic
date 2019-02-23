#pragma once

#include <Tempest/Matrix4x4>
#include "physics/dynamicworld.h"
#include "graphics/staticobjects.h"
#include "graphics/protomesh.h"
#include "game/inventory.h"

class Npc;
class World;
class Trigger;

class Interactive final {
  public:
    enum Anim : uint8_t {
      In,
      Active,
      Out
      };

    Interactive(World& owner,const ZenLoad::zCVobData &vob);
    Interactive(Interactive&&)=default;

    std::array<float,3> position() const;
    std::array<float,3> displayPosition() const;
    const char*         displayName() const;

    std::string         stateFunc() const;
    const Trigger*      triggerTarget() const;

    bool                isContainer() const;
    Inventory&          inventory();

    bool attach (Npc& npc);
    void dettach(Npc& npc);

    const char* anim(Anim t) const;
    void marchInteractives(Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

    Tempest::Matrix4x4     objMat;

  private:
    struct Pos final {
      Npc*               user=nullptr;
      std::string        name;
      size_t             node=0;
      Tempest::Matrix4x4 pos;
      };

    void setPos(Npc& npc,std::array<float,3> pos);
    void setDir(Npc& npc,const Tempest::Matrix4x4& mt);
    void attach(Npc& npc,Pos& to);
    void implAddItem(char *name);

    World*             world = nullptr;
    ZenLoad::zCVobData data;
    Inventory          invent;
    int                state=0;

    std::vector<Pos>    pos;
    const ProtoMesh*    mesh = nullptr;
    StaticObjects::Mesh view;
    DynamicWorld::Item  physic;
  };
