#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include "physicmesh.h"

#include "graphics/mesh/attachbinder.h"
#include "graphics/mesh/skeleton.h"
#include "graphics/mesh/pose.h"

PhysicMesh::PhysicMesh(const ProtoMesh& proto, DynamicWorld& owner)
  :ani(&proto) {
  Tempest::Matrix4x4 pos;
  pos.identity();

  for(auto& i:proto.attach) {
    auto physic = owner.staticObj(i.shape.get(),pos);
    sub.emplace_back(std::move(physic));
    }
  }

void PhysicMesh::setObjMatrix(const Tempest::Matrix4x4& mt) {
  if(binder!=nullptr){
    for(size_t i=0;i<binder->bind.size();++i){
      auto id=binder->bind[i];
      if(id>=skeleton->tr.size())
        continue;
      auto subI = ani->submeshId[i].id;
      auto mat=mt;
      mat.translate(ani->rootTr[0],ani->rootTr[1],ani->rootTr[2]);
      mat.mul(skeleton->tr[id]);
      sub[subI].setObjMatrix(mat);
      }
    } else {
    for(auto& i:sub)
      i.setObjMatrix(mt);
    }
  }

void PhysicMesh::setSkeleton(const Skeleton* sk) {
  skeleton = sk;
  if(ani!=nullptr && skeleton!=nullptr)
    binder=Resources::bindMesh(*ani,*skeleton);
  }

void PhysicMesh::setPose(const Pose& p, const Tempest::Matrix4x4& obj) {
  if(binder!=nullptr){
    for(size_t i=0;i<binder->bind.size();++i){
      auto id=binder->bind[i];
      if(id>=p.transform().size())
        continue;
      auto mat=obj;
      mat.mul(p.transform(id));

      auto subI = ani->submeshId[i].id;
      sub[subI].setObjMatrix(mat);
      }
    }
  }
