#include "skeleton.h"

using namespace Tempest;

Skeleton::Skeleton(const ZenLoad::zCModelMeshLib &src) {
  nodes.resize(src.getNodes().size());
  tr.resize(src.getNodes().size());

  for(size_t i=0;i<nodes.size();++i) {
    Node& n = nodes[i];
    auto& s = src.getNodes()[i];

    n.name   = s.name;
    n.parent = s.parentIndex==uint16_t(-1) ? size_t(-1) : s.parentIndex;
    std::memcpy(&n.tr,&s.transformLocal,sizeof(n.tr));
    }
  mkSkeleton();
  }

void Skeleton::mkSkeleton() {
  Matrix4x4 m;
  m.identity();
  mkSkeleton(m,size_t(-1));
  }

void Skeleton::mkSkeleton(const Tempest::Matrix4x4 &mt, size_t parent) {
  for(size_t i=0;i<nodes.size();++i){
    if(nodes[i].parent!=parent)
      continue;
    tr[i] = mt;
    tr[i].mul(nodes[i].tr);
    mkSkeleton(tr[i],i);
    }
  }
