#include "collisionworld.h"
#include "physicvbo.h"

#include "physics/physics.h"
#include "dynamicworld.h"
#include "world/objects/item.h"

CollisionWorld::CollisionBody::CollisionBody(btRigidBody::btRigidBodyConstructionInfo& inf, CollisionWorld* owner)
  :btRigidBody(inf), owner(owner) {
  }

CollisionWorld::CollisionBody::~CollisionBody() {
  auto flags = this->getCollisionFlags();

  if((flags & btCollisionObject::CF_STATIC_OBJECT)==0) {
    owner->removeRigidBody(this);
    auto& rigid = owner->rigid;
    for(size_t i=0; i<rigid.size(); ++i) {
      if(rigid[i]==this) {
        rigid[i] = rigid.back();
        rigid.pop_back();
        break;
        }
      }
    } else {
    owner->removeCollisionObject(this);
    }

  owner->touchAabbs();
  }

struct CollisionWorld::Broadphase : btDbvtBroadphase {
  struct BroadphaseRayTester : btDbvt::ICollide {
    btBroadphaseRayCallback& m_rayCallback;
    BroadphaseRayTester(btBroadphaseRayCallback& orgCallback) : m_rayCallback(orgCallback) {}
    void Process(const btDbvtNode* leaf) {
      btDbvtProxy* proxy = reinterpret_cast<btDbvtProxy*>(leaf->data);
      m_rayCallback.process(proxy);
      }
    };

  Broadphase() {
    m_deferedcollide = true;
    rayTestStk.reserve(btDbvt::DOUBLE_STACKSIZE);
    }

  void rayTest(const btVector3& rayFrom, const btVector3& rayTo, btBroadphaseRayCallback& rayCallback,
               const btVector3& aabbMin, const btVector3& aabbMax) {
    BroadphaseRayTester callback(rayCallback);
    btAlignedObjectArray<const btDbvtNode*>* stack = &rayTestStk;

    m_sets[0].rayTestInternal(m_sets[0].m_root,
        rayFrom,
        rayTo,
        rayCallback.m_rayDirectionInverse,
        rayCallback.m_signs,
        rayCallback.m_lambda_max,
        aabbMin,
        aabbMax,
        *stack,
        callback);

    m_sets[1].rayTestInternal(m_sets[1].m_root,
        rayFrom,
        rayTo,
        rayCallback.m_rayDirectionInverse,
        rayCallback.m_signs,
        rayCallback.m_lambda_max,
        aabbMin,
        aabbMax,
        *stack,
        callback);
    }

  btAlignedObjectArray<const btDbvtNode*> rayTestStk;
  };

struct CollisionWorld::ContructInfo {
  ContructInfo() {
    // collision configuration contains default setup for memory, collision setup
    conf  .reset(new btDefaultCollisionConfiguration());
    // use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
    //auto disp = new btCollisionDispatcherMt(conf.get());
    disp  .reset(new btCollisionDispatcher(conf.get()));
    // disp->setDispatcherFlags(btCollisionDispatcher::CD_DISABLE_CONTACTPOOL_DYNAMIC_ALLOCATION); // may crash with OOM, when runout of pool
    broad .reset(new Broadphase());
    solver.reset(new btSequentialImpulseConstraintSolver());
    }
  std::unique_ptr<btCollisionConfiguration>            conf;
  std::unique_ptr<btCollisionDispatcher>               disp;
  std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
  std::unique_ptr<btBroadphaseInterface>               broad;
  };

CollisionWorld::CollisionWorld()
  :CollisionWorld(ContructInfo()) {
  }

btVector3 CollisionWorld::toMeters(const ZMath::float3& v) {
  return {v.x*0.01f, v.y*0.01f, v.z*0.01f};
  }

btVector3 CollisionWorld::toMeters(const Tempest::Vec3& v) {
  return {v.x*0.01f, v.y*0.01f, v.z*0.01f};
  }

const Tempest::Vec3 CollisionWorld::toCentimeters(const btVector3& v) {
  return {v.x()*100.f, v.y()*100.f, v.z()*100.f};
  }

CollisionWorld::CollisionWorld(ContructInfo ci)
  :btDiscreteDynamicsWorld(ci.disp.get(), ci.broad.get(), ci.solver.get(), ci.conf.get()) {
  disp   = std::move(ci.disp);
  broad  = std::move(ci.broad);
  solver = std::move(ci.solver);
  conf   = std::move(ci.conf);

  setForceUpdateAllAabbs(false);
  gravity = btVector3(0,-DynamicWorld::gravityMS,0);
  setGravity(gravity);
  }

void CollisionWorld::updateAabbs() {
  if(aabbChanged>0) {
    btDiscreteDynamicsWorld::updateAabbs();
    aabbChanged = 0;
    return;
    }
  }

void CollisionWorld::touchAabbs() {
  aabbChanged++;
  }

bool CollisionWorld::hasCollision(btRigidBody& it, Tempest::Vec3& normal) {
  struct rCallBack : public btCollisionWorld::ContactResultCallback {
    int                 count=0;
    Tempest::Vec3       norm={};
    btCollisionObject*  src=nullptr;

    explicit rCallBack(btCollisionObject* src):src(src){
      m_collisionFilterMask = btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::StaticFilter;
      }

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
      auto obj=reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject);
      if(obj->getUserIndex()!=DynamicWorld::C_Water &&
         obj->getUserIndex()!=DynamicWorld::C_Ghost &&
         obj->getUserIndex()!=DynamicWorld::C_Item)
        return ContactResultCallback::needsCollision(proxy0);
      return false;
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
      norm /= norm.length();
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
  }

std::unique_ptr<CollisionWorld::CollisionBody> CollisionWorld::addCollisionBody(btCollisionShape& shape, const Tempest::Matrix4x4& tr, float friction) {
  btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
        0,                  // mass, in kg. 0 -> Static object, will never move.
        nullptr,
        &shape,
        btVector3(0,0,0)
        );

  std::unique_ptr<CollisionBody> obj(new CollisionBody(rigidBodyCI,this));
  obj->setFlags(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_NO_CONTACT_RESPONSE);
  obj->setCollisionFlags(btCollisionObject::CO_COLLISION_OBJECT);

  btTransform trans;
  trans.setFromOpenGLMatrix(tr.data());
  trans.getOrigin()*=0.01f;

  obj->setWorldTransform(trans);
  obj->setFriction(friction);

  this->addCollisionObject(obj.get());
  this->updateSingleAabb(obj.get());
  return obj;
  }

std::unique_ptr<CollisionWorld::DynamicBody> CollisionWorld::addDynamicBody(btCollisionShape& shape, const Tempest::Matrix4x4& tr,
                                                                            float friction, float mass) {
  if(mass<=0)
    mass = 1;
  btVector3 localInertia = {};
  shape.calculateLocalInertia(mass, localInertia);

  btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
        mass,
        nullptr,
        &shape,
        localInertia
        );

  std::unique_ptr<DynamicBody> obj(new DynamicBody(rigidBodyCI,this));
  // obj->setFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
  // obj->setCollisionFlags(btCollisionObject::CO_RIGID_BODY);

  btTransform trans;
  trans.setFromOpenGLMatrix(tr.data());
  trans.getOrigin()*=0.01f;

  obj->setWorldTransform(trans);
  obj->setFriction(friction);
  obj->setActivationState(ACTIVE_TAG);

  obj->setCcdSweptSphereRadius(0.1f);
  obj->setCcdMotionThreshold(0.005f);

  this->addRigidBody(obj.get());
  this->updateSingleAabb(obj.get());
  rigid.push_back(obj.get());
  return obj;
  }

void CollisionWorld::rayCast(const Tempest::Vec3& b, const Tempest::Vec3& e, btCollisionWorld::RayResultCallback& cb) {
  btVector3 s = toMeters(b), f = toMeters(e);
  if(s==f)
    return;
  this->rayTest(s,f,cb);
  }

void CollisionWorld::tick(uint64_t dt) {
  static bool  dynamic = true;
  const  float dtF     = float(dt);

  if(dynamic) {
    if(rigid.size()>0)
      this->stepSimulation(dtF/1000.f, 2);

    if(hitItem) {
      const int numManifolds = getDispatcher()->getNumManifolds();
      for(int i=0; i<numManifolds; ++i) {
        btPersistentManifold* contactManifold = getDispatcher()->getManifoldByIndexInternal(i);
        const btCollisionObject* a = contactManifold->getBody0();
        const btCollisionObject* b = contactManifold->getBody1();

        for(auto obj:{a,b}) {
          if(obj->getUserIndex()!=DynamicWorld::C_Item)
            continue;
          const int numContacts = contactManifold->getNumContacts();
          if(numContacts==0)
            continue;

          btManifoldPoint& pt = contactManifold->getContactPoint(0);
          auto impulse = pt.getAppliedImpulse();
          auto mass    = reinterpret_cast<const DynamicBody*>(obj)->mass;
          if(impulse/mass<0.9f)
            continue;

          auto land = (obj==a ? b : a);
          if(land->getUserIndex()!=DynamicWorld::C_Landscape)
            continue;

          auto matId = ZenLoad::STONE;
          if(auto shape = land->getCollisionShape()) {
            auto s  = reinterpret_cast<const btMultimaterialTriangleMeshShape*>(shape);
            auto mt = reinterpret_cast<const PhysicVbo*>(s->getMeshInterface());

            int part = (obj==a ? pt.m_partId1 : pt.m_partId0);
            matId = ZenLoad::MaterialGroup(mt->materialId(size_t(part)));
            }

          if(auto ptr = reinterpret_cast<::Item*>(obj->getUserPointer())) {
            hitItem(*ptr,matId,impulse,mass);
            }
          }
        }
      }
    } else {
    // fake 'just fall' implementation
    for(auto& i:rigid) {
      i->setLinearVelocity(i->getLinearVelocity()+gravity*dtF);

      uint64_t cnt    = uint64_t(i->getLinearVelocity().length());
      bool     active = cnt>0;
      for(uint64_t r=1; active && r<=(cnt*dt) && r<150; ++r)
        active &= tick(1.f/float(cnt*dt),*i);

      if(active) {
        i->setDeactivationTime(0);
        i->setActivationState(ACTIVE_TAG);
        } else {
        i->setLinearVelocity(btVector3(0,0,0));
        i->setActivationState(WANTS_DEACTIVATION);
        i->setDeactivationTime(i->getDeactivationTime()+float(dt)/1000.f);
        }
      }
    }

  for(auto i:rigid)
    if(auto ptr = reinterpret_cast<::Item*>(i->getUserPointer())) {
      auto t = i->getWorldTransform();
      t.getOrigin()*=100.f;
      Tempest::Matrix4x4 mt;
      t.getOpenGLMatrix(reinterpret_cast<btScalar*>(&mt));
      ptr->setObjMatrix(mt);
      }
  for(size_t i=0; i<rigid.size(); ++i) {
    auto it = rigid[i];
    if((it->wantsSleeping() && (it->getDeactivationTime()>3.f || !it->isActive())) ||
       (it->getWorldTransform().getOrigin().y()<bbox[0].y()-100)) {
      if(auto ptr = reinterpret_cast<::Item*>(it->getUserPointer()))
        ptr->setPhysicsDisable();
      }
    }
  }

void CollisionWorld::setBBox(const btVector3& min, const btVector3& max) {
  bbox[0] = min;
  bbox[1] = max;
  }

void CollisionWorld::setItemHitCallback(std::function<void(Item&, ZenLoad::MaterialGroup, float, float)> f) {
  hitItem = f;
  }

bool CollisionWorld::tick(float step, btRigidBody& body) {
  Tempest::Vec3 norm;

  btTransform prev = body.getWorldTransform();
  auto  trans = prev;
  auto& at    = trans.getOrigin();

  at+=body.getLinearVelocity()*step;
  body.setWorldTransform(trans);

  if(hasCollision(body,norm)) {
    body.setWorldTransform(prev);
    return false;
    }
  updateSingleAabb(&body);
  return true;
  }

void CollisionWorld::saveKinematicState(btScalar /*timeStep*/) {
  // assume no CF_KINEMATIC_OBJECT in this game
  }

