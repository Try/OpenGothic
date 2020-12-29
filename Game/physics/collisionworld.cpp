#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include "collisionworld.h"

#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "dynamicworld.h"

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
    conf .reset(new btDefaultCollisionConfiguration());
    // use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
    //auto disp = new btCollisionDispatcherMt(conf.get());
    disp .reset(new btCollisionDispatcher(conf.get()));
    disp->setDispatcherFlags(btCollisionDispatcher::CD_DISABLE_CONTACTPOOL_DYNAMIC_ALLOCATION);
    broad.reset(new btDbvtBroadphase());
    }
  std::unique_ptr<btCollisionConfiguration>   conf;
  std::unique_ptr<btCollisionDispatcher>      disp;
  std::unique_ptr<btBroadphaseInterface>      broad;
  };

CollisionWorld::CollisionWorld()
  :CollisionWorld(ContructInfo()){
  }

CollisionWorld::CollisionWorld(ContructInfo ci)
  :btCollisionWorld(ci.disp.get(), ci.broad.get(), ci.conf.get()) {
  disp  = std::move(ci.disp);
  broad = std::move(ci.broad);
  conf  = std::move(ci.conf);

  setForceUpdateAllAabbs(false);
  gravity = btVector3(0,-DynamicWorld::gravity*1000.f,0);
  //world->setGravity();
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

void CollisionWorld::addRigidBody(btRigidBody* body) {
  addCollisionObject(body);
  rigid.push_back(body);
  }

void CollisionWorld::removeRigidBody(btRigidBody* body) {
  removeCollisionObject(body);
  for(size_t i=0; i<rigid.size(); ++i) {
    if(rigid[i]==body) {
      rigid[i] = rigid.back();
      rigid.pop_back();
      return;
      }
    }
  }

void CollisionWorld::tick(uint64_t dt) {
  const float dtF = float(dt);

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
