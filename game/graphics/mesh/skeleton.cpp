#include "skeleton.h"

#include <cassert>

#include "utils/fileext.h"
#include "resources.h"

using namespace Tempest;

Skeleton::Skeleton(const ZenLoad::zCModelMeshLib &src, const Animation* anim, const char* name)
  :fileName(name), anim(anim) {
  bboxCol[0] = src.getBBoxCollisionMin();
  bboxCol[1] = src.getBBoxCollisionMax();

  nodes.resize(src.getNodes().size());
  tr.resize(src.getNodes().size());

  for(size_t i=0;i<nodes.size();++i) {
    Node& n = nodes[i];
    auto& s = src.getNodes()[i];

    n.name   = s.name;
    n.parent = s.parentIndex==uint16_t(-1) ? size_t(-1) : s.parentIndex;
    std::memcpy(reinterpret_cast<void*>(&n.tr),reinterpret_cast<const void*>(&s.transformLocal),sizeof(n.tr));
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

  auto tr = src.getRootNodeTranslation();
  rootTr = Vec3{tr.x,tr.y,tr.z};

  for(auto& i:nodes)
    if(i.parent==size_t(-1)){
      i.tr.translate(rootTr);
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

const std::string& Skeleton::defaultMesh() const {
  if(anim)
    return anim->defaultMesh();
  static std::string nop;
  return nop;
  }

float Skeleton::colisionHeight() const {
  return std::fabs(bboxCol[1].y-bboxCol[0].y);
  }

float Skeleton::colisionRadius() const {
  float x = std::fabs(bboxCol[1].x-bboxCol[0].x);
  float y = std::fabs(bboxCol[1].z-bboxCol[0].z);
  return std::max(x,y); //TODO
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
