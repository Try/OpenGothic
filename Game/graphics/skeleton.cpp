#include "skeleton.h"

#include <cassert>
#include "resources.h"

using namespace Tempest;

Skeleton::Skeleton(const ZenLoad::zCModelMeshLib &src, std::string meshLib)
  :meshLib(std::move(meshLib)){
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

  for(size_t i=0;i<nodes.size();++i) {
    if(nodes[i].parent>=i && nodes[i].parent!=size_t(-1)) {
      ordered=false;
      break;
      }
    }
  for(size_t i=0;i<nodes.size();++i)
    if(nodes[i].parent==size_t(-1))
      rootNodes.push_back(i);

  anim = Resources::loadAnimation(this->meshLib);

  auto tr = src.getRootNodeTranslation();
  rootTr = {{tr.x,tr.y,tr.z}};

  for(auto& i:nodes)
    if(i.parent==size_t(-1)){
      i.tr.translate(rootTr[0],rootTr[1],rootTr[2]);
      }
  mkSkeleton();
  }

size_t Skeleton::findNode(const char *name, size_t def) const {
  if(name==nullptr)
    return def;
  for(size_t i=0;i<nodes.size();++i)
    if(nodes[i].name==name)
      return i;
  return def;
  }

size_t Skeleton::findNode(const std::string &name, size_t def) const {
  return findNode(name.c_str(),def);
  }

const Animation::Sequence* Skeleton::sequence(const char *name) const {
  if(anim)
    return anim->sequence(name);
  return nullptr;
  }

void Skeleton::debug() const {
  if(anim)
    anim->debug();
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
