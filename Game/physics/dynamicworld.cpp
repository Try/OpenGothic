#include "dynamicworld.h"

#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <LinearMath/btDefaultMotionState.h>

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
  world->addRigidBody(landBody.get());
  }

DynamicWorld::~DynamicWorld(){
  world->removeRigidBody(landBody.get());
  }

float DynamicWorld::dropRay(float x, float y, float z, bool &hasCol) const {
  btVector3 s(x,y+150,z), e(x,y-20000,z);
  btCollisionWorld::ClosestRayResultCallback callback{s,e};

  world->rayTest(s,e,callback);
  hasCol = callback.hasHit();

  if(callback.hasHit())
    return callback.m_hitPointWorld.y();
  return y;
  }

float DynamicWorld::dropRay(float x,float y,float z) const {
  bool unused;
  return dropRay(x,y,z,unused);
  }
