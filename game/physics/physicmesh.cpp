#include "physicmesh.h"

#include "graphics/mesh/attachbinder.h"
#include "graphics/mesh/skeleton.h"
#include "graphics/mesh/pose.h"
#include "graphics/mesh/protomesh.h"

PhysicMesh::PhysicMesh(const ProtoMesh& proto, DynamicWorld& owner, bool movable)
  :ani(&proto) {
  Tempest::Matrix4x4 pos;
  pos.identity();

  for(auto& i:proto.attach) {
    DynamicWorld::Item physic;
    if(movable)
      physic = owner.movableObj(i.shape.get(),pos); else
      physic = owner.staticObj (i.shape.get(),pos);
    sub.emplace_back(std::move(physic));
    }
  }

bool PhysicMesh::isEmpty() const {
  return sub.empty();
  }

void PhysicMesh::setObjMatrix(const Tempest::Matrix4x4& obj) {
  implSetObjMatrix(obj,skeleton==nullptr ? nullptr : skeleton->tr.data());
  }

void PhysicMesh::setSkeleton(const Skeleton* sk) {
  skeleton = sk;
  if(ani!=nullptr && skeleton!=nullptr)
    binder=Resources::bindMesh(*ani,*skeleton);
  }

void PhysicMesh::setPose(const Pose& p, const Tempest::Matrix4x4& obj) {
  implSetObjMatrix(obj,p.transform());
  }

void PhysicMesh::implSetObjMatrix(const Tempest::Matrix4x4 &mt, const Tempest::Matrix4x4* tr) {
  const size_t binds = (binder==nullptr ? 0 : binder->bind.size());
  for(size_t i=0; i<binds; ++i) {
    if(ani->submeshId[i].subId!=0)
      continue;
    auto id  = binder->bind[i];
    auto mat = mt;
    if(id<skeleton->tr.size())
      mat.mul(tr[id]);
    sub[ani->submeshId[i].id].setObjMatrix(mat);
    }
  for(size_t i=binds; i<sub.size(); ++i)
    sub[i].setObjMatrix(mt);
  }

void PhysicMesh::setInteractive(Interactive* it) {
  for(auto& i:sub)
    i.setInteractive(it);
  }
