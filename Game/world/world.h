#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Matrix4x4>
#include <string>

#include <daedalus/DaedalusVM.h>
#include <zenload/zTypes.h>
#include <zenload/zTypes.h>

#include "graphics/worldview.h"
#include "graphics/staticobjects.h"
#include "physics/dynamicworld.h"
#include "worldscript.h"
#include "resources.h"
#include "npc.h"

class Gothic;
class RendererStorage;
class DynamicWorld;

class World final {
  public:
    World()=default;
    World(const World&)=delete;
    World(Gothic &gothic,const RendererStorage& storage, std::string file);

    struct Block {
      const Tempest::Texture2d*      texture = nullptr;
      Tempest::IndexBuffer<uint32_t> ibo;
      };

    struct Dodad {
      const ProtoMesh*               mesh = nullptr;
      Tempest::Matrix4x4             objMat;
      };

    bool isEmpty() const { return wname.empty(); }
    const std::string& name() const { return wname; }

    const Tempest::VertexBuffer<Resources::Vertex>& landVbo()    const { return vbo; }
    const std::vector<Block>&                       landBlocks() const { return blocks; }

    std::vector<Dodad> staticObj;

    const ZenLoad::zCWaypointData* findPoint(const std::string& s) const { return findPoint(s.c_str()); }
    const ZenLoad::zCWaypointData* findPoint(const char* name) const;

    WorldView*    view()   const { return wview.get(); }
    DynamicWorld* physic() const { return wdynamic.get(); }

    StaticObjects::Mesh getView(const std::string& visual);
    StaticObjects::Mesh getView(const std::string& visual, int32_t headTex, int32_t teetTex, int32_t bodyColor);

    void updateAnimation();
    Npc* player() const { return npcPlayer; }

    void tick(uint64_t dt);
    uint64_t tickCount() const;

  private:
    std::string                           wname;
    Gothic*                               gothic=nullptr;
    ZenLoad::zCWayNetData                 wayNet;
    std::vector<ZenLoad::zCWaypointData>  freePoints, startPoints;

    Tempest::VertexBuffer<Resources::Vertex> vbo;
    std::vector<Block>                       blocks;

    Npc*                                  npcPlayer=nullptr;

    std::unique_ptr<WorldView>            wview;
    std::unique_ptr<DynamicWorld>         wdynamic;
    std::unique_ptr<WorldScript>          vm;

    void    loadVob(const ZenLoad::zCVobData &vob);
    void    addStatic(const ZenLoad::zCVobData &vob);

    void    initScripts(bool firstTime);
    int32_t runFunction(const std::string &fname,bool clearDataStack);
  };
