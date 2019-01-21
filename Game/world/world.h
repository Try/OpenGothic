#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Matrix4x4>
#include <string>

#include <daedalus/DaedalusVM.h>
#include <zenload/zTypes.h>
#include <zenload/zTypes.h>

#include "graphics/staticobjects.h"
#include "resources.h"
#include "npc.h"
#include "worldscript.h"

class Gothic;

class World final {
  public:
    World()=default;
    World(Gothic &gothic, std::string file);

    struct Block {
      Tempest::Texture2d*            texture=nullptr;
      Tempest::IndexBuffer<uint32_t> ibo;
      };

    struct Dodad {
      StaticMesh*                    mesh   =nullptr;
      Tempest::Matrix4x4             objMat;
      };

    bool isEmpty() const { return name.empty(); }

    const Tempest::VertexBuffer<Resources::Vertex>& landVbo() const { return vbo; }
    const std::vector<Block>& landBlocks() const { return blocks; }

    std::vector<Dodad> staticObj;

    const ZenLoad::zCWaypointData* findPoint(const std::string& s) const { return findPoint(s.c_str()); }
    const ZenLoad::zCWaypointData* findPoint(const char* name) const;

  private:
    std::string                           name;
    Gothic*                               gothic=nullptr;
    std::unique_ptr<WorldScript>          vm;
    ZenLoad::zCWayNetData                 wayNet;
    // StaticObjects                         visualObj;

    Tempest::VertexBuffer<Resources::Vertex> vbo;

    std::vector<Block>                       blocks;

    void    loadVob(const ZenLoad::zCVobData &vob);

    void    initScripts(bool firstTime);
    int32_t runFunction(const std::string &fname,bool clearDataStack);
  };
