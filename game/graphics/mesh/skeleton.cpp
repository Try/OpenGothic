#include "skeleton.h"

#include <cassert>

#include "utils/fileext.h"
#include "resources.h"

using namespace Tempest;

Skeleton::Skeleton(const phoenix::model_hierarchy& src, const Animation* anim, std::string_view name)
      :fileName(name), anim(anim) {
    bboxCol[0] = {src.collision_bbox.min.x, src.collision_bbox.min.y, src.collision_bbox.min.z};
    bboxCol[1] = {src.collision_bbox.max.x, src.collision_bbox.max.y, src.collision_bbox.max.z};

    nodes.resize(src.nodes.size());
    tr.resize(src.nodes.size());

    for(size_t i=0;i<nodes.size();++i) {
      Node& n = nodes[i];
      auto& s = src.nodes[i];

      n.name   = s.name;
      n.parent = s.parent_index == -1 ? size_t(-1) : size_t(s.parent_index);

      auto transposed_transform = s.transform;
      std::memcpy(reinterpret_cast<void*>(&n.tr),reinterpret_cast<const void*>(&transposed_transform),sizeof(n.tr));
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

    auto tr = src.root_translation;
    rootTr = Vec3{tr.x,tr.y,tr.z};

    for(auto& i:nodes)
      if(i.parent==size_t(-1)){
        i.tr.translate(rootTr);
      }
    BIP01_HEAD = findNode("BIP01 HEAD");
    mkSkeleton();
  }

size_t Skeleton::findNode(std::string_view name, size_t def) const {
  if(name.empty())
    return def;
  for(size_t i=0;i<nodes.size();++i)
    if(nodes[i].name==name)
      return i;
  return def;
  }

const Animation::Sequence* Skeleton::sequence(std::string_view name) const {
  if(anim!=nullptr)
    return anim->sequence(name);
  return nullptr;
  }

void Skeleton::debug() const {
  if(anim!=nullptr)
    anim->debug();
  }

std::string_view Skeleton::defaultMesh() const {
  if(anim!=nullptr)
    return anim->defaultMesh();
  return "";
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
