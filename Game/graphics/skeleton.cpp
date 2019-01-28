#include "skeleton.h"

#include <cassert>
#include "resources.h"

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
  assert(nodes.size()<=Resources::MAX_NUM_SKELETAL_NODES);
  for(auto& i:tr)
    i.identity();
  mkSkeleton();

  auto tr = src.getRootNodeTranslation();
  rootTr = {{tr.x,tr.y,tr.z}};
  }

void Skeleton::mkSkeleton() {
  Matrix4x4 m;
  m.identity();
  m.translate(rootTr[0],rootTr[1],rootTr[2]);
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
