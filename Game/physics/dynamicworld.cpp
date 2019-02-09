#include "dynamicworld.h"

#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <LinearMath/btDefaultMotionState.h>

#include <cmath>

DynamicWorld::DynamicWorld(World&, const ZenLoad::zCMesh &mesh) {
  // collision configuration contains default setup for memory, collision setup
  conf.reset(new btDefaultCollisionConfiguration());
  //conf->setConvexConvexMultipointIterations();

  // use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
  dispatcher.reset(new btCollisionDispatcher(conf.get()));

  broadphase.reset(new btDbvtBroadphase());

  // the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
  solver.reset(new btSequentialImpulseConstraintSolver());

  world.reset(new btDiscreteDynamicsWorld(dispatcher.get(),broadphase.get(),solver.get(),conf.get()));
  world->setGravity(btVector3(0,-10,0));

  auto& id = mesh.getIndices();
  auto& v  = mesh.getVertices();
  landIndex.resize(id.size());
  for(size_t i=0;i<id.size();i+=3){
    int*  d = &landIndex[i];
    auto* s = &id[i];
    d[0] = int(s[0]);
    d[1] = int(s[1]);
    d[2] = int(s[2]);
    }
  landVert.resize(v.size()*4);
  for(size_t i=0;i<v.size();++i){
    auto* d=&landVert[i*4];
    auto& s=v[i];
    d[0]=s.x;
    d[1]=s.y;
    d[2]=s.z;
    }

  btTriangleMesh* tmesh=new btTriangleMesh();
  landMesh.reset(tmesh);
  for(size_t i=0;i<id.size();i+=3){
    int*  d  = &landIndex[i];
    auto& v0 = v[d[0]];
    auto& v1 = v[d[1]];
    auto& v2 = v[d[2]];

    btVector3 v[] = {{v0.x, v0.y, v0.z},
                     {v1.x, v1.y, v1.z},
                     {v2.x, v2.y, v2.z}};
    tmesh->addTriangle(v[0],v[1],v[2]);
    }

  //btTriangleIndexVertexArray vx;
  //landMesh.reset(new btTriangleIndexVertexArray(int(landIndex.size())/3,landIndex.data(),sizeof(landIndex[0]),
  //                                              int(landVert.size()),   landVert.data(), sizeof(landVert[0])*3));

  landShape.reset(new btBvhTriangleMeshShape(landMesh.get(),true,true));
  btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
        0,                  // mass, in kg. 0 -> Static object, will never move.
        nullptr,
        landShape.get(),
        btVector3(0,0,0)
        );
  landBody.reset(new btRigidBody(rigidBodyCI));
  landBody->setUserIndex(C_Landscape);
  world->addRigidBody(landBody.get());
  }

DynamicWorld::~DynamicWorld(){
  world->removeRigidBody(landBody.get());
  }

float DynamicWorld::dropRay(float x,float y,float z) const {
  bool unused;
  return dropRay(x,y,z,unused);
  }

std::array<float,3> DynamicWorld::ray(float x0, float y0, float z0, float x1, float y1, float z1) const {
  bool unused;
  return ray(x0,y0,z0,x1,y1,z1,unused);
  }

float DynamicWorld::dropRay(float x, float y, float z, bool &hasCol) const {
  return ray(x,y+150,z, x,y-20000,z,hasCol)[1];
  }

std::array<float,3> DynamicWorld::ray(float x0, float y0, float z0, float x1, float y1, float z1, bool &hasCol) const {
  struct CallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  btVector3 s(x0,y0,z0), e(x1,y1,z1);
  CallBack callback{s,e};

  world->rayTest(s,e,callback);
  hasCol = callback.hasHit();

  if(callback.hasHit()){
    return {{callback.m_hitPointWorld.x(),callback.m_hitPointWorld.y(),callback.m_hitPointWorld.z()}};
    }
  return {x1,y1,z1};
  }

DynamicWorld::Item DynamicWorld::ghostObj() {
  btGhostObject*    obj   = new btGhostObject();
  btCollisionShape* shape = new btCapsuleShape(20,70);
  obj->setCollisionShape(shape);

  btTransform trans;
  trans.setIdentity();
  obj->setWorldTransform(trans);
  obj->setUserIndex(C_Ghost);

  world->addCollisionObject(obj);

  return Item(this,obj);
  }

void DynamicWorld::deleteObj(btGhostObject *obj) {
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
    trans.setOrigin(btVector3(x,y+120,z));
    obj->setWorldTransform(trans);
    }
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
  setPosition(pos[0],pos[1],pos[2]);
  const bool ret=owner->hasCollision(*this,norm);
  if(ret){
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
