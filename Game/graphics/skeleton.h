#pragma once

#include <Tempest/Matrix4x4>
#include <zenload/zCModelMeshLib.h>

#include <vector>

class Animation;

class Skeleton final {
  public:
    Skeleton(const ZenLoad::zCModelMeshLib & src,std::string meshLib);

    struct Node final {
      size_t             parent=size_t(-1);
      Tempest::Matrix4x4 tr;
      std::string        name;
      };
    std::vector<Node>               nodes;
    std::vector<Tempest::Matrix4x4> tr;
    std::array<float,3>             rootTr;

    size_t                          findNode(const char*        name,size_t def=size_t(-1)) const;
    size_t                          findNode(const std::string& name,size_t def=size_t(-1)) const;

  private:
    std::string      meshLib;
    const Animation* anim=nullptr;

    void mkSkeleton();
    void mkSkeleton(const Tempest::Matrix4x4& mt,size_t parent);
  };
