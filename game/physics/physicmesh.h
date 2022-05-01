#pragma once

#include "dynamicworld.h"

class Pose;
class ProtoMesh;
class Skeleton;
class AttachBinder;

class PhysicMesh final {
  public:
    PhysicMesh()=default;
    PhysicMesh(const ProtoMesh& proto, DynamicWorld& owner, bool movable);

    bool   isEmpty() const;

    void   setObjMatrix  (const Tempest::Matrix4x4& m);
    void   setSkeleton   (const Skeleton* sk);
    void   setPose       (const Pose&      p);
    void   setInteractive(Interactive* it);

  private:
    void   implSetObjMatrix(const Tempest::Matrix4x4& mt, const Tempest::Matrix4x4* tr);

    std::vector<DynamicWorld::Item> sub;
    const ProtoMesh*                ani=nullptr;
    const Skeleton*                 skeleton=nullptr;
    const AttachBinder*             binder=nullptr;
  };
