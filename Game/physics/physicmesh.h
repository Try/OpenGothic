#pragma once

#include "dynamicworld.h"

class Pose;

class PhysicMesh final {
  public:
    PhysicMesh()=default;
    PhysicMesh(const ProtoMesh& proto, DynamicWorld& owner);

    void   setObjMatrix  (const Tempest::Matrix4x4& m);
    void   setSkeleton   (const Skeleton* sk);
    void   setPose       (const Pose&      p,const Tempest::Matrix4x4& obj);

  private:
    std::vector<DynamicWorld::StaticItem> sub;
    const ProtoMesh*                      ani=nullptr;
    const Skeleton*                       skeleton=nullptr;
    const AttachBinder*                   binder=nullptr;
  };
