#pragma once

#include <Tempest/Matrix4x4>
#include <zenload/zCModelMeshLib.h>

#include <vector>

class Skeleton final {
  public:
    Skeleton(const ZenLoad::zCModelMeshLib & src);

    struct Node final {
      size_t             parent=size_t(-1);
      Tempest::Matrix4x4 tr;
      std::string        name;
      };
    std::vector<Node>               nodes;
    std::vector<Tempest::Matrix4x4> tr;
    std::array<float,3>             rootTr;

    void mkSkeleton();

  private:
    void mkSkeleton(const Tempest::Matrix4x4& mt,size_t parent);
  };
