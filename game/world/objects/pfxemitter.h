#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

#include "world/collisionzone.h"
#include "physics/dynamicworld.h"
#include "graphics/meshobjects.h"
#include "graphics/pfx/trlobjects.h"

class World;
class PfxBucket;
class ParticleFx;
class PfxObjects;
class PfxEmitterMesh;

class PfxEmitter {
  public:
    PfxEmitter()=default;
    PfxEmitter(World& world, std::string_view name);
    PfxEmitter(World& world, const ParticleFx* decl);
    PfxEmitter(PfxObjects& obj, const ParticleFx* vob);
    PfxEmitter(World& world, const phoenix::vob& vob);
    ~PfxEmitter();
    PfxEmitter(PfxEmitter&&);
    PfxEmitter& operator=(PfxEmitter&& b);
    PfxEmitter(const PfxEmitter&)=delete;

    bool     isEmpty() const { return bucket==nullptr; }
    void     setPosition (float x,float y,float z);
    void     setPosition (const Tempest::Vec3& pos);
    void     setDirection(const Tempest::Matrix4x4& pos);
    void     setObjMatrix(const Tempest::Matrix4x4& mt);
    void     setActive(bool act);
    bool     isActive() const;
    void     setLooped(bool loop);
    void     setMesh(const MeshObjects::Mesh* mesh, const Pose* pose);

    void     setTarget(const Npc* tg);

    void     setPhysicsEnable (World& physic, std::function<void(Npc& npc)> cb);
    void     setPhysicsDisable();

    uint64_t effectPrefferedTime() const;
    bool     isAlive() const;

  private:
    PfxEmitter(PfxBucket &b,size_t id);

    PfxBucket* bucket = nullptr;
    size_t     id     = size_t(-1);

    CollisionZone      zone;
    TrlObjects::Item   trail;
    MeshObjects::Mesh  shpMesh;

  friend class PfxBucket;
  friend class PfxObjects;
  };

