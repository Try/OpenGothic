#pragma once

#include <Tempest/Matrix4x4>
#include "graphics/protomesh.h"

class Npc;
class World;

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

    bool attach (Npc& npc);
    void dettach(Npc& npc);

    const char* anim(Anim t) const;
    void marchInteractives(Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

    const ProtoMesh*       mesh = nullptr;
    const PhysicMeshShape* physic = nullptr;
    Tempest::Matrix4x4     objMat;

  private:
    struct Pos final {
      Npc*               user=nullptr;
      std::string        name;
      size_t             node=0;
      Tempest::Matrix4x4 pos;
      };

    void setPos(Npc &npc,std::array<float,3> pos);
    void setDir(Npc &npc,const Tempest::Matrix4x4& mt);
    void attach (Npc& npc,Pos& to);

    World*             world = nullptr;
    ZenLoad::zCVobData data;

    std::vector<Pos>   pos;
  };
