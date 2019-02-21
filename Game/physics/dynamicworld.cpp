#include "dynamicworld.h"
#include "physicmeshshape.h"

#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btSimpleDynamicsWorld.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <LinearMath/btDefaultMotionState.h>

#include <cmath>

const float DynamicWorld::ghostPadding=10;
const float DynamicWorld::worldHeight =20000;

DynamicWorld::DynamicWorld(World&,const ZenLoad::PackedMesh& pkg) {
  // collision configuration contains default setup for memory, collision setup
  conf.reset(new btDefaultCollisionConfiguration());
  // use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
  dispatcher.reset(new btCollisionDispatcher(conf.get()));
  broadphase.reset(new btDbvtBroadphase());
  // the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
  world.reset(new btCollisionWorld(dispatcher.get(),broadphase.get(),conf.get()));

  landMesh.reset(new PhysicMesh(pkg.vertices));
  for(auto& i:pkg.subMeshes)
    if(!i.material.noCollDet && i.indices.size()>0)
      landMesh->addIndex(i.indices);

  landShape.reset(new btBvhTriangleMeshShape(landMesh.get(),false,true));
  btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
        0,                  // mass, in kg. 0 -> Static object, will never move.
        nullptr,
        landShape.get(),
        btVector3(0,0,0)
        );
  landBody.reset(new btRigidBody(rigidBodyCI));
  landBody->setUserIndex(C_Landscape);
  world->addCollisionObject(landBody.get());
  }

DynamicWorld::~DynamicWorld(){
  world->removeCollisionObject(landBody.get());
  }

float DynamicWorld::dropRay(float x,float y,float z) const {
  bool unused;
  return dropRay(x,y,z,unused);
  }

std::array<float,3> DynamicWorld::landNormal(float x, float y, float z) const {
  struct CallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  btVector3 s(x,y+50,z), e(x,y-worldHeight,z);
  CallBack callback{s,e};

  world->rayTest(s,e,callback);

  if(callback.hasHit())
    return {{callback.m_hitNormalWorld.x(),callback.m_hitNormalWorld.y(),callback.m_hitNormalWorld.z()}};
  return {0,1,0};
  }

std::array<float,3> DynamicWorld::ray(float x0, float y0, float z0, float x1, float y1, float z1) const {
  bool unused;
  return ray(x0,y0,z0,x1,y1,z1,unused);
  }

float DynamicWorld::dropRay(float x, float y, float z, bool &hasCol) const {
  return ray(x,y+50,z, x,y-worldHeight,z,hasCol)[1];
  }

std::array<float,3> DynamicWorld::ray(float x0, float y0, float z0, float x1, float y1, float z1, bool &hasCol) const {
  struct CallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      const int idx = rayResult.m_collisionObject->getUserIndex();
      if(idx==C_Ghost)
        return false;
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  btVector3 s(x0,y0,z0), e(x1,y1,z1);
  CallBack callback{s,e};

  world->rayTest(s,e,callback);
  hasCol = callback.hasHit();

  if(callback.hasHit())
    return {{callback.m_hitPointWorld.x(),callback.m_hitPointWorld.y(),callback.m_hitPointWorld.z()}};
  return {x1,y1,z1};
  }

DynamicWorld::Item DynamicWorld::ghostObj(float r,float height) {
  btCollisionShape*  shape = new btCapsuleShape(r*0.5f,140*0.5f);
  btGhostObject*     obj   = new btGhostObject();
  obj->setCollisionShape(shape);
  //btCollisionObject* obj   = new btRigidBody(0,nullptr,shape);

  btTransform trans;
  trans.setIdentity();
  obj->setWorldTransform(trans);
  obj->setUserIndex(C_Ghost);

  //obj->setCollisionFlags(obj->getCollisionFlags()|btCollisionObject::CF_KINEMATIC_OBJECT);
  world->addCollisionObject(obj);

  return Item(this,obj);
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
  std::unique_ptr<btRigidBody> body(new btRigidBody(rigidBodyCI));
  body->setUserIndex(C_Landscape);

  btTransform trans;
  trans.setFromOpenGLMatrix(m.data());
  body->setWorldTransform(trans);

  world->addCollisionObject(body.get());
  return Item(this,body.release());
  }

void DynamicWorld::tick(uint64_t /*dt*/) {
  world->updateAabbs();
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

    btScalar addSingleResult(btManifoldPoint& p,
                             const btCollisionObjectWrapper*, int, int,
                             const btCollisionObjectWrapper*, int, int){
      norm[0]+=p.m_normalWorldOnB.x();
      norm[1]+=p.m_normalWorldOnB.y();
      norm[2]+=p.m_normalWorldOnB.z();
      ++count;
      return 0;
      }

    void normalize(){
      float l = std::sqrt(norm[0]*norm[0]+norm[1]*norm[1]+norm[2]*norm[2]);
      norm[0]/=l;
      norm[1]/=l;
      norm[2]/=l;
      }
    };

  rCallBack callback;
  world->contactTest(it.obj, callback);

  if(callback.count>0){
    callback.normalize();
    normal=callback.norm;
    }
  return callback.count>0;
  }

void DynamicWorld::Item::setPosition(float x, float y, float z) {
  if(obj){
    btTransform trans;
    trans.setIdentity();
    trans.setOrigin(btVector3(x,y+90,z));
    obj->setWorldTransform(trans);
    }
  }

void DynamicWorld::Item::setObjMatrix(const Tempest::Matrix4x4 &m) {
  if(obj){
    btTransform trans;
    trans.setFromOpenGLMatrix(reinterpret_cast<const btScalar*>(&m));
    obj->setWorldTransform(trans);
    }
  }

bool DynamicWorld::Item::tryMove(const std::array<float,3> &pos) {
  if(!obj)
    return false;
  std::array<float,3> tmp={};
  auto                tr = obj->getWorldTransform();
  if(owner->hasCollision(*this,tmp))
    return true;
  setPosition(pos[0],pos[1],pos[2]);
  const bool ret=owner->hasCollision(*this,tmp);
  obj->setWorldTransform(tr);
  return !ret;
  }

bool DynamicWorld::Item::tryMove(const std::array<float,3> &pos,
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

bool DynamicWorld::Item::hasCollision() const {
  if(!obj)
    return false;
  std::array<float,3> tmp;
  return owner->hasCollision(*this,tmp);
  }
