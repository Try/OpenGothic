#include "dynamicworld.h"

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

struct DynamicWorld::PhyMesh:btTriangleIndexVertexArray {
  PhyMesh(const std::vector<ZMath::float3>& v,
          const std::vector<uint32_t>& index)
    :PhyMesh(v){
    addIndex(index);
    }

  PhyMesh(const std::vector<ZMath::float3>& v)
    :vert(v.size()) {
    for(size_t i=0;i<v.size();++i){
      vert[i].setValue(v[i].x,v[i].y,v[i].z);
      }
    }

  PhyMesh(const PhyMesh&)=delete;
  PhyMesh(PhyMesh&&)=delete;

  void addIndex(const std::vector<uint32_t>& index){
    size_t off=id.size();
    id.insert(id.end(),index.begin(),index.end());

    btIndexedMesh meshIndex={};
    meshIndex.m_numTriangles = id.size()/3;
    meshIndex.m_numVertices  = int32_t(vert.size());

    meshIndex.m_indexType           = PHY_INTEGER;
    meshIndex.m_triangleIndexBase   = reinterpret_cast<const uint8_t*>(&id[0]);
    meshIndex.m_triangleIndexStride = 3 * sizeof(uint32_t);

    meshIndex.m_vertexBase          = reinterpret_cast<const uint8_t*>(&vert[0]);
    meshIndex.m_vertexStride        = sizeof(btVector3);

    m_indexedMeshes.push_back(meshIndex);
    segments.push_back(Segment{off,int(index.size()/3)});
    adjustMesh();
    }

  void adjustMesh(){
    for(int i=0;i<m_indexedMeshes.size();++i){
      btIndexedMesh& meshIndex=m_indexedMeshes[i];
      Segment&       sg       =segments[size_t(i)];

      meshIndex.m_triangleIndexBase = reinterpret_cast<const uint8_t*>(&id[sg.off]);
      meshIndex.m_numTriangles      = sg.size;
      }
    }

  struct Segment {
    size_t off;
    int    size;
    };

  std::vector<btVector3> vert;
  std::vector<uint32_t>  id;
  std::vector<Segment>   segments;
  };

DynamicWorld::DynamicWorld(World&, const ZenLoad::zCMesh &mesh) {
  // collision configuration contains default setup for memory, collision setup
  conf.reset(new btDefaultCollisionConfiguration());
  // use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
  dispatcher.reset(new btCollisionDispatcher(conf.get()));
  broadphase.reset(new btDbvtBroadphase());
  // the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
  world.reset(new btCollisionWorld(dispatcher.get(),broadphase.get(),conf.get()));

  auto& id = mesh.getIndices();
  auto& v  = mesh.getVertices();

  PhyMesh* tmesh = new PhyMesh(v,id);
  landMesh.reset(tmesh);

  landShape.reset(new btBvhTriangleMeshShape(landMesh.get(),true,true));
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

std::array<float,3> DynamicWorld::ray(float x0, float y0, float z0, float x1, float y1, float z1) const {
  bool unused;
  return ray(x0,y0,z0,x1,y1,z1,unused);
  }

float DynamicWorld::dropRay(float x, float y, float z, bool &hasCol) const {
  return ray(x,y+50,z, x,y-20000,z,hasCol)[1];
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
