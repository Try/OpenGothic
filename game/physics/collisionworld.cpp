#include <Tempest/Platform>

#include "collisionworld.h"
#include "dynamicworld.h"
#include "physicvbo.h"

using namespace reactphysics3d;

bool CollisionWorld::RayCallback::onHit(Collider* obj, size_t submeshId, const Tempest::Vec3& pos, const Tempest::Vec3& norm) {
  (void)pos;
  (void)norm;
  return true;
  }

CollisionWorld::CollisionWorld() {
  PhysicsWorld::WorldSettings settings;
  settings.defaultVelocitySolverNbIterations = 20;
  settings.isSleepingEnabled                 = true;
  settings.gravity                           = Vector3(0,-DynamicWorld::gravity*1000.f,0);

  world = common.createPhysicsWorld(settings);
  }

CollisionWorld::~CollisionWorld() {
  common.destroyPhysicsWorld(world);
  }

CollisionShape* CollisionWorld::createConcaveMeshShape(const PhysicVbo* pvbo) {
  auto& s = meshes[pvbo];
  if(s!=nullptr)
    return s;
  s = pvbo->mkMesh(common);
  return s;
  }

CollisionShape* CollisionWorld::createConcaveMeshShape(TriangleMesh* mesh) {
  return common.createConcaveMeshShape(mesh);
  }

CollisionShape* CollisionWorld::createBoxShape(const Tempest::Vec3& sz) {
  return common.createBoxShape({sz.x,sz.y,sz.z});
  }

void CollisionWorld::deleteShape(CollisionShape* shp) {
  auto t = shp->getName();
  switch(t) {
    case CollisionShapeName::TRIANGLE:
      // ???
      break;
    case CollisionShapeName::SPHERE:
      common.destroySphereShape(reinterpret_cast<SphereShape*>(shp));
      break;
    case CollisionShapeName::CAPSULE:
      common.destroyCapsuleShape(reinterpret_cast<CapsuleShape*>(shp));
      break;
    case CollisionShapeName::BOX:
      common.destroyBoxShape(reinterpret_cast<BoxShape*>(shp));
      break;
    case CollisionShapeName::CONVEX_MESH:
      common.destroyConvexMeshShape(reinterpret_cast<ConvexMeshShape*>(shp));
      break;
    case CollisionShapeName::TRIANGLE_MESH: {
      common.destroyConcaveMeshShape(reinterpret_cast<ConcaveMeshShape*>(shp));
      break;
      }
    case CollisionShapeName::HEIGHTFIELD:
      common.destroyHeightFieldShape(reinterpret_cast<HeightFieldShape*>(shp));
      break;
    }
  }

CollisionBody* CollisionWorld::createStatic(CollisionShape* shp, const reactphysics3d::Transform& tr, uint16_t group) {
  auto ret = world->createCollisionBody(tr);
  auto c   = ret->addCollider(shp,Transform::identity());
  c->setCollideWithMaskBits(group);
  return ret;
  }

CollisionBody* CollisionWorld::createMovable(CollisionShape* shp, const Transform& tr, uint16_t group) {
  auto ret = world->createCollisionBody(tr);
  auto c   = ret->addCollider(shp,Transform::identity());
  c->setCollideWithMaskBits(group);
  // ret->setType(BodyType::STATIC);
  return ret;
  }

CollisionBody* CollisionWorld::createDynamic(CollisionShape* shp, const Transform& tr, uint16_t group) {
  auto ret = world->createRigidBody(tr);
  auto c   = ret->addCollider(shp,Transform::identity());
  // c->setMaterial();
  c->setCollideWithMaskBits(group);
  ret->setType(BodyType::DYNAMIC);
  return ret;
  }

void CollisionWorld::deleteObj(CollisionBody* obj) {
  if(obj==nullptr)
    return;
  if(auto r = dynamic_cast<RigidBody*>(obj))
    world->destroyRigidBody(r); else
    world->destroyCollisionBody(obj);
  }

void CollisionWorld::tick(uint64_t dt) {
  float dtF = float(dt)/1000.f;
  world->update(dtF);
  }

void CollisionWorld::raycast(const Tempest::Vec3& from, const Tempest::Vec3& to, CollisionWorld::RayCallback& fn) {
  Vector3 s(from.x, from.y, from.z);
  Vector3 e(to.x,   to.y,   to.z);
  Ray ray(s, e);

  struct Callback : public RaycastCallback {
    decimal notifyRaycastHit(const RaycastInfo& info) override {
      Tempest::Vec3 worldPoint (info.worldPoint.x, info.worldPoint.y, info.worldPoint.z);
      Tempest::Vec3 worldNormal(info.worldNormal.x,info.worldNormal.y,info.worldNormal.z);

      const bool ret = fn->onHit(info.collider,info.meshSubpart,worldPoint,worldNormal);
      return ret ? 0.f : 1.f;
      }

    CollisionWorld::RayCallback* fn = nullptr;
    } cb;
  cb.fn = &fn;

  world->raycast(ray,&cb);
  }

bool CollisionWorld::hasCollision(reactphysics3d::CollisionBody& it, Tempest::Vec3& normal) {
  struct rCallback:CollisionCallback {
    Tempest::Vec3 normal      = {};
    bool          hasContacts = false;
    void onContact(const CallbackData& callbackData) override {
      hasContacts = true;
      normal      = Tempest::Vec3(); // TODO
      }
    };
  rCallback callback;
  world->testCollision(&it,callback);
  normal = callback.normal;
  return callback.hasContacts;

  /*
  struct rCallBack : public btCollisionWorld::ContactResultCallback {
    int                 count=0;
    Tempest::Vec3       norm={};
    btCollisionObject*  src=nullptr;

    explicit rCallBack(btCollisionObject* src):src(src){
      m_collisionFilterMask = btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::StaticFilter;
      }

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
      auto obj=reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject);
      if(obj->getUserIndex()!=DynamicWorld::C_Water)
        return ContactResultCallback::needsCollision(proxy0);
      return false;
      return true;
      }

    btScalar addSingleResult(btManifoldPoint& p,
                             const btCollisionObjectWrapper*, int, int,
                             const btCollisionObjectWrapper*, int, int) override {
      norm.x+=p.m_normalWorldOnB.x();
      norm.y+=p.m_normalWorldOnB.y();
      norm.z+=p.m_normalWorldOnB.z();
      ++count;
      return 0;
      }

    void normalize() {
      norm /= norm.manhattanLength();
      }
    };

  rCallBack callback{&it};

  updateAabbs();
  contactTest(&it, callback);

  if(callback.count>0){
    callback.normalize();
    normal=callback.norm;
    }
  return callback.count>0;
  */
  }
