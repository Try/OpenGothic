#include "collisionworld.h"

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
    disp->setDispatcherFlags(btCollisionDispatcher::CD_DISABLE_CONTACTPOOL_DYNAMIC_ALLOCATION);
    broad .reset(new btDbvtBroadphase());
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

CollisionWorld::CollisionWorld(ContructInfo ci)
  :btDiscreteDynamicsWorld(ci.disp.get(), ci.broad.get(), ci.solver.get(), ci.conf.get()) {
  disp   = std::move(ci.disp);
  broad  = std::move(ci.broad);
  solver = std::move(ci.solver);
  conf   = std::move(ci.conf);

  setForceUpdateAllAabbs(false);
  gravity = btVector3(0,-DynamicWorld::gravityMS*1000.f,0);
  setGravity(gravity);
  }

void CollisionWorld::updateAabbs() {
  if(aabbChanged>0) {
    btCollisionWorld::updateAabbs();
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
  obj->setWorldTransform(trans);
  obj->setFriction(friction);

  this->addCollisionObject(obj.get());
  this->updateSingleAabb(obj.get());
  return obj;
  }

std::unique_ptr<CollisionWorld::DynamicBody> CollisionWorld::addDynamicBody(btCollisionShape& shape, const Tempest::Matrix4x4& tr, float friction, float mass) {
  if(mass<=0)
    mass = 10;
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
  //obj->setCollisionFlags(btCollisionObject::CO_RIGID_BODY);

  btTransform trans;
  trans.setFromOpenGLMatrix(tr.data());
  obj->setWorldTransform(trans);
  obj->setFriction(friction);
  obj->setActivationState(ACTIVE_TAG);

  this->addRigidBody(obj.get());
  this->updateSingleAabb(obj.get());
  rigid.push_back(obj.get());
  return obj;
  }

void CollisionWorld::rayCast(const Tempest::Vec3& b, const Tempest::Vec3& e, btCollisionWorld::RayResultCallback& cb) {
  btVector3 s(b.x,b.y,b.z), f(e.x,e.y,e.z);
  // cb.m_rayFromWorld = s;
  // cb.m_rayToWorld   = f;
  this->rayTest(s,f,cb);
  }

void CollisionWorld::tick(uint64_t dt) {
  static bool  dynamic = false;
  const  float dtF     = float(dt);

  if(dynamic) {
    this->stepSimulation(dtF/1000.f, 0, 5.f/60.f);
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
      auto& t = i->getWorldTransform();
      Tempest::Matrix4x4 mt;
      t.getOpenGLMatrix(reinterpret_cast<btScalar*>(&mt));
      ptr->setMatrix(mt);
      }
  for(size_t i=0; i<rigid.size(); ++i) {
    auto it = rigid[i];
    if(it->wantsSleeping() && (it->getDeactivationTime()>3.f || !it->isActive())) {
      if(auto ptr = reinterpret_cast<::Item*>(it->getUserPointer()))
        ptr->setPhysicsDisable();
      }
    }
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

