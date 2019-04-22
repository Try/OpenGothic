#include "dynamicworld.h"
#include "physicmeshshape.h"

#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcherMt.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/BroadphaseCollision/btSimpleBroadphase.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btSimpleDynamicsWorld.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/CollisionShapes/btConeShape.h>
#include <BulletCollision/CollisionShapes/btMultimaterialTriangleMeshShape.h>
#include <LinearMath/btDefaultMotionState.h>
#include <BulletCollision/NarrowPhaseCollision/btRaycastCallback.h>

#include <zenload/zCMaterial.h>

#include <cmath>

const float DynamicWorld::ghostPadding=50-22.5f;
const float DynamicWorld::ghostHeight =140;
const float DynamicWorld::worldHeight =20000;

struct DynamicWorld::HumShape:btCapsuleShape {
  HumShape(btScalar radius, btScalar height):btCapsuleShape(height<=0.f ? 0.f : radius,height){}

  // "human" object mush have identyty scale/rotation matrix. Only translation allowed.
  void getAabb(const btTransform& t, btVector3& aabbMin, btVector3& aabbMax) const override {
    const btScalar rad = getRadius();
    btVector3      extent(rad,rad,rad);
    extent[m_upAxis]  = rad + getHalfHeight();
    btVector3  center = t.getOrigin();

    aabbMin = center - extent;
    aabbMax = center + extent;
    }
  };

struct Broadphase : btDbvtBroadphase {
  btAlignedObjectArray<const btDbvtNode*> rayTestStk;

  struct BroadphaseRayTester : btDbvt::ICollide {
    btBroadphaseRayCallback& m_rayCallback;
    BroadphaseRayTester(btBroadphaseRayCallback& orgCallback) : m_rayCallback(orgCallback) {}
    void Process(const btDbvtNode* leaf) {
      btDbvtProxy* proxy = reinterpret_cast<btDbvtProxy*>(leaf->data);
      m_rayCallback.process(proxy);
      }
    };

  Broadphase() {
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
  };

DynamicWorld::DynamicWorld(World&,const ZenLoad::PackedMesh& pkg) {
  // collision configuration contains default setup for memory, collision setup
  conf.reset(new btDefaultCollisionConfiguration());

  // use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
  auto disp = new btCollisionDispatcherMt(conf.get());
  //auto disp = new btCollisionDispatcher(conf.get());
  dispatcher.reset(disp);
  disp->setDispatcherFlags(btCollisionDispatcher::CD_DISABLE_CONTACTPOOL_DYNAMIC_ALLOCATION);

  Broadphase* brod = new Broadphase();
  broadphase.reset(brod);
  // the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
  world.reset(new btCollisionWorld(dispatcher.get(),broadphase.get(),conf.get()));

  landMesh.reset(new PhysicMesh(pkg.vertices));
  for(auto& i:pkg.subMeshes)
    if(!i.material.noCollDet && i.indices.size()>0 && i.material.matGroup!=ZenLoad::MaterialGroup::WATER)
      landMesh->addIndex(i.indices,i.material.matGroup);

  //landShape.reset(new btBvhTriangleMeshShape(landMesh.get(),false,true));
  landShape.reset(new btMultimaterialTriangleMeshShape(landMesh.get(),false,true));
  landBody = landObj();

  world->addCollisionObject(landBody.get());
  world->setForceUpdateAllAabbs(false);
  }

DynamicWorld::~DynamicWorld(){
  world->removeCollisionObject(landBody.get());
  }

DynamicWorld::RayResult DynamicWorld::dropRay(float x,float y,float z) const {
  bool unused;
  return dropRay(x,y,z,unused);
  }

std::array<float,3> DynamicWorld::landNormal(float x, float y, float z) const {
  struct rCallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;

    rCallBack(const btVector3& rayFromWorld, const btVector3& rayToWorld)
      :ClosestRayResultCallback(rayFromWorld,rayToWorld){
      m_collisionFilterMask = btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::StaticFilter;
      }

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
      if(reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject)->getUserIndex()==C_Landscape)
        return ClosestRayResultCallback::needsCollision(proxy0);
      return false;
      }
    };

  btVector3 s(x,y+50,z), e(x,y-worldHeight,z);
  rCallBack callback{s,e};

  rayTest(s,e,callback);

  if(callback.hasHit())
    return {{callback.m_hitNormalWorld.x(),callback.m_hitNormalWorld.y(),callback.m_hitNormalWorld.z()}};
  return {0,1,0};
  }

DynamicWorld::RayResult DynamicWorld::dropRay(float x, float y, float z, bool &hasCol) const {
  if(lastRayDrop.hasCol && lastRayDropXyz[0]==x && lastRayDropXyz[1]==y && lastRayDropXyz[2]==z){
    hasCol = true;
    return lastRayDrop;
    }
  lastRayDropXyz[0] = x;
  lastRayDropXyz[1] = y;
  lastRayDropXyz[2] = z;
  lastRayDrop       = ray(x,y+ghostPadding,z, x,y-worldHeight,z);
  hasCol            = lastRayDrop.hasCol;
  return lastRayDrop;
  }

DynamicWorld::RayResult DynamicWorld::ray(float x0, float y0, float z0, float x1, float y1, float z1) const {
  struct CallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;
    uint8_t matId = 0;

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
      auto obj=reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject);
      if(obj->getUserIndex()==C_Landscape)
        return ClosestRayResultCallback::needsCollision(proxy0);
      return false;
      }

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      auto shape = rayResult.m_collisionObject->getCollisionShape();
      if(shape) {
        auto s  = reinterpret_cast<const btMultimaterialTriangleMeshShape*>(shape);
        auto mt = reinterpret_cast<const PhysicMesh*>(s->getMeshInterface());

        size_t id = size_t(rayResult.m_localShapeInfo->m_shapePart);
        matId = mt->getMaterialId(id);
        }
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  btVector3 s(x0,y0,z0), e(x1,y1,z1);
  CallBack callback{s,e};
  callback.m_flags = btTriangleRaycastCallback::kF_KeepUnflippedNormal | btTriangleRaycastCallback::kF_FilterBackfaces;

  rayTest(s,e,callback);
  //hasCol = callback.hasHit();

  if(callback.hasHit())
    return RayResult{{callback.m_hitPointWorld.x(),callback.m_hitPointWorld.y(),callback.m_hitPointWorld.z()},callback.matId,true};
  return RayResult{{x1,y1,z1},callback.matId,false};
  }

std::unique_ptr<btRigidBody> DynamicWorld::landObj() {
  btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
        0,                  // mass, in kg. 0 -> Static object, will never move.
        nullptr,
        landShape.get(),
        btVector3(0,0,0)
        );
  std::unique_ptr<btRigidBody> obj(new btRigidBody(rigidBodyCI));
  obj->setUserIndex(C_Landscape);
  obj->setFlags(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_NO_CONTACT_RESPONSE);
  obj->setCollisionFlags(btCollisionObject::CO_RIGID_BODY);
  return obj;
  }

DynamicWorld::Item DynamicWorld::ghostObj(const ZMath::float3 &min, const ZMath::float3 &max) {
  static const float dimMax=45.f;
  float dx     = max.x-min.x;
  float dz     = max.z-min.z;
  float dim    = std::max(dx,dz);
  float height = max.y-min.y;

  if(dim>dimMax)
    dim=dimMax;

  btCollisionShape* shape = new HumShape(dim*0.5f,std::max(height-ghostPadding,0.f)*0.5f);
  btRigidBody*      obj   = new btRigidBody(0,nullptr,shape);//new btGhostObject();
  obj->setCollisionShape(shape);

  btTransform trans;
  trans.setIdentity();
  obj->setWorldTransform(trans);
  obj->setUserIndex(C_Ghost);
  obj->setFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
  obj->setCollisionFlags(btCollisionObject::CO_RIGID_BODY);

  world->addCollisionObject(obj);
  return Item(this,obj,height,dim*0.5f);
  }

DynamicWorld::Item DynamicWorld::staticObj(const PhysicMeshShape *shape, const Tempest::Matrix4x4 &m) {
  if(shape==nullptr)
    return Item();

  btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
        0,                  // mass, in kg. 0 -> Static object, will never move.
        nullptr,
        &shape->shape,
        btVector3(0,0,0)
        );
  std::unique_ptr<btRigidBody> obj(new btRigidBody(rigidBodyCI));
  obj->setUserIndex(C_Landscape);
  obj->setFlags(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_NO_CONTACT_RESPONSE);
  obj->setCollisionFlags(btCollisionObject::CO_RIGID_BODY);

  btTransform trans;
  trans.setFromOpenGLMatrix(m.data());
  obj->setWorldTransform(trans);

  world->addCollisionObject(obj.get());
  return Item(this,obj.release(),0,0);
  }

void DynamicWorld::tick(uint64_t /*dt*/) {
  //world->updateAabbs();
  }

void DynamicWorld::updateSingleAabb(btCollisionObject *obj) {
  //dirtyAabb.push_back(obj);
  world->updateSingleAabb(obj);
  }

void DynamicWorld::deleteObj(btCollisionObject *obj) {
  if(!obj)
    return;
  world->removeCollisionObject(obj);
  delete obj;
  }

bool DynamicWorld::hasCollision(const Item& it,std::array<float,3>& normal) {
  struct rCallBack : public btCollisionWorld::ContactResultCallback {
    int                 count=0;
    std::array<float,3> norm={};
    btCollisionObject*  src=nullptr;

    explicit rCallBack(btCollisionObject* src):src(src){
      m_collisionFilterMask = btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::StaticFilter;
      }

    btScalar addSingleResult(btManifoldPoint& p,
                             const btCollisionObjectWrapper*, int, int,
                             const btCollisionObjectWrapper*, int, int) override {
      norm[0]+=p.m_normalWorldOnB.x();
      norm[1]+=p.m_normalWorldOnB.y();
      norm[2]+=p.m_normalWorldOnB.z();
      ++count;
      return 0;
      }

    void normalize() {
      float l = std::sqrt(norm[0]*norm[0]+norm[1]*norm[1]+norm[2]*norm[2]);
      norm[0]/=l;
      norm[1]/=l;
      norm[2]/=l;
      }
    };

  rCallBack callback{it.obj};
  world->contactTest(it.obj, callback);

  if(callback.count>0){
    callback.normalize();
    normal=callback.norm;
    }
  return callback.count>0;
  }

void DynamicWorld::rayTest(const btVector3 &s,
                           const btVector3 &e,
                           btCollisionWorld::RayResultCallback &callback) const {
  if(s==e)
    return;

  if(/* DISABLES CODE */ (1)){
    world->rayTest(s,e,callback);
    } else {
    btTransform rayFromTrans,rayToTrans;
    rayFromTrans.setIdentity();
    rayFromTrans.setOrigin(s);
    rayToTrans.setIdentity();
    rayToTrans.setOrigin(e);
    world->rayTestSingle(rayFromTrans, rayToTrans, landBody.get(),
                         landBody->getCollisionShape(),
                         landBody->getWorldTransform(),
                         callback);
    }
  }

void DynamicWorld::Item::setPosition(float x, float y, float z) {
  if(obj) {
    implSetPosition(x,y,z);
    owner->updateSingleAabb(obj);
    }
  }

void DynamicWorld::Item::implSetPosition(float x, float y, float z) {
  btTransform trans;
  trans.setIdentity();
  trans.setOrigin(btVector3(x,y+(height-r-ghostPadding)*0.5f+r+ghostPadding,z));
  // trans.setOrigin(btVector3(x,y+(ghostHeight-ghostPadding)*0.5f+ghostPadding,z));
  obj->setWorldTransform(trans);
  }

void DynamicWorld::Item::setObjMatrix(const Tempest::Matrix4x4 &m) {
  if(obj){
    btTransform trans;
    trans.setFromOpenGLMatrix(reinterpret_cast<const btScalar*>(&m));
    obj->setWorldTransform(trans);
    owner->updateSingleAabb(obj);
    }
  }

void DynamicWorld::Item::setEnable(bool e) {
  if(obj){
    obj->setUserIndex(e ? C_Ghost : C_Null);
    }
  }

bool DynamicWorld::Item::testMove(const std::array<float,3> &pos) {
  if(!obj)
    return false;
  std::array<float,3> tmp={};
  if(owner->hasCollision(*this,tmp))
    return true;
  auto tr = obj->getWorldTransform();
  implSetPosition(pos[0],pos[1],pos[2]);
  const bool ret=owner->hasCollision(*this,tmp);
  obj->setWorldTransform(tr);
  return !ret;
  }

bool DynamicWorld::Item::testMove(const std::array<float,3> &pos,
                                  std::array<float,3>       &fallback,
                                  float speed) {
  fallback=pos;
  if(!obj)
    return false;

  std::array<float,3> norm={};
  auto                tr = obj->getWorldTransform();
  if(owner->hasCollision(*this,norm))
    return true;
  //auto ground = dropRay(pos[0],pos[1],pos[2]);
  setPosition(pos[0],pos[1],pos[2]);
  const bool ret=owner->hasCollision(*this,norm);
  if(ret && speed!=0.f){
    fallback[0] = pos[0] + norm[0]*speed;
    fallback[1] = pos[1];// - norm[1]*speed;
    fallback[2] = pos[2] + norm[2]*speed;
    }
  obj->setWorldTransform(tr);
  return !ret;
  }

bool DynamicWorld::Item::tryMoveN(const std::array<float,3> &pos, std::array<float,3> &norm) {
  norm = {};

  if(!obj)
    return false;
  auto tr = obj->getWorldTransform();
  if(owner->hasCollision(*this,norm)){
    setPosition(pos[0],pos[1],pos[2]);
    return true;
    }

  implSetPosition(pos[0],pos[1],pos[2]);
  const bool ret=owner->hasCollision(*this,norm);
  if(!ret) {
    owner->updateSingleAabb(obj);
    return true;
    }

  obj->setWorldTransform(tr);
  return false;
  }

bool DynamicWorld::Item::tryMove(const std::array<float,3> &pos, std::array<float,3> &fallback, float speed) {
  fallback=pos;
  if(!obj)
    return false;

  std::array<float,3> norm={};
  auto                tr = obj->getWorldTransform();
  if(owner->hasCollision(*this,norm)){
    setPosition(pos[0],pos[1],pos[2]);
    return true;
    }

  implSetPosition(pos[0],pos[1],pos[2]);
  const bool ret=owner->hasCollision(*this,norm);
  if(!ret) {
    owner->updateSingleAabb(obj);
    return true;
    }
  if(speed!=0.f){
    fallback[0] = pos[0] + norm[0]*speed;
    fallback[1] = pos[1];// - norm[1]*speed;
    fallback[2] = pos[2] + norm[2]*speed;
    }
  obj->setWorldTransform(tr);
  return false;
  }

bool DynamicWorld::Item::hasCollision() const {
  if(!obj)
    return false;
  std::array<float,3> tmp;
  return owner->hasCollision(*this,tmp);
  }
