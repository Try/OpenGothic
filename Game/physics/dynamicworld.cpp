#include "dynamicworld.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

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
#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <BulletCollision/NarrowPhaseCollision/btRaycastCallback.h>
#include <LinearMath/btDefaultMotionState.h>
#include <LinearMath/btScalar.h>

#include "graphics/submesh/packedmesh.h"
#include "physicmeshshape.h"
#include "physicmesh.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>

#include "world/bullet.h"

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

struct DynamicWorld::NpcBody : btRigidBody {
  NpcBody(btCollisionShape* shape):btRigidBody(0,nullptr,shape){}

  std::array<float,3> pos={};
  float               r=0, h=0, rX=0, rZ=0;
  bool                enable=true;
  bool                frozen=false;
  uint64_t            lastMove=0;

  Npc* getNpc() {
    return reinterpret_cast<Npc*>(getUserPointer());
    }

  bool setPosition(float x,float y,float z){
    return setPosition({x,y,z});
    }

  bool setPosition(const std::array<float,3>& p){
    if(p==pos)
      return false;
    pos = p;
    //updateAaBb = true;

    btTransform trans;
    trans.setIdentity();
    trans.setOrigin(btVector3(pos[0],pos[1]+(h-r-ghostPadding)*0.5f+r+ghostPadding,pos[2]));
    setWorldTransform(trans);
    return true;
    }
  };

struct DynamicWorld::NpcBodyList final {
  struct Record final {
    NpcBody* body=nullptr;
    float    x   =0.f;
    };

  NpcBodyList(DynamicWorld& wrld):wrld(wrld){
    body.reserve(1024);
    frozen.reserve(1024);
    }

  void add(NpcBody* b){
    Record r;
    r.body = b;
    r.x    = b->pos[0];
    body.push_back(r);
    }

  bool del(void* b){
    if(del(b,body))
      return true;
    if(del(b,frozen)){
      srt=false;
      return true;
      }
    return false;
    }

  bool del(void* b,std::vector<Record>& arr){
    for(size_t i=0;i<arr.size();++i){
      if(arr[i].body!=b)
        continue;
      arr[i]=arr.back();
      arr.pop_back();
      return true;
      }
    return false;
    }

  bool delMisordered(void* b,std::vector<Record>& arr){
    for(size_t i=0;i<arr.size();++i){
      if(arr[i].body!=b)
        continue;
      const float x = arr[i].x;
      if((i==0 || arr[i-1].x<x) &&
         (i+1==arr.size() || x<arr[i+1].x)) {
        arr[i].x = arr[i].body->pos[0];
        return false;
        }
      arr[i].body = nullptr;
      return true;
      }
    return false;
    }

  void resize(NpcBody& n, float h, float dx, float dz){
    n.rX = dx;
    n.rZ = dz;

    //n.r = std::max(dx,dz)*0.5f;
    n.r = (dx+dz)*0.25f;
    n.h = h;

    maxR = std::max(maxR,n.r);
    }

  void move(NpcBody& n, const std::array<float,3>& pos){
    const bool move = n.setPosition(pos);
    if(move)
      n.lastMove=tick;

    if(move && n.frozen){
      if(delMisordered(&n,frozen)){
        Record r;
        r.body = &n;
        r.x    = 0.f;
        body.push_back(r);
        n.frozen=false;
        }
      }
    }

  void move(NpcBody& n, float x, float y, float z){
    move(n,{x,y,z});
    }

  bool rayTest(NpcBody& npc, const btVector3& s, const btVector3& e) {
    struct CallBack:btCollisionWorld::ClosestRayResultCallback {
      using ClosestRayResultCallback::ClosestRayResultCallback;
      };
    CallBack callback{s,e};

    btTransform rayFromTrans,rayToTrans;
    rayFromTrans.setIdentity();
    rayFromTrans.setOrigin(s);
    rayToTrans.setIdentity();
    rayToTrans.setOrigin(e);

    return rayTestSingle(rayFromTrans, rayToTrans, npc, callback);
    }

  NpcBody* rayTest(const btVector3& s, const btVector3& e) {
    struct CallBack:btCollisionWorld::ClosestRayResultCallback {
      using ClosestRayResultCallback::ClosestRayResultCallback;
      };
    CallBack callback{s,e};

    btTransform rayFromTrans,rayToTrans;
    rayFromTrans.setIdentity();
    rayFromTrans.setOrigin(s);
    rayToTrans.setIdentity();
    rayToTrans.setOrigin(e);
    for(auto i:body)
      if(rayTestSingle(rayFromTrans, rayToTrans, *i.body, callback))
        return i.body;
    for(auto i:frozen)
      if(i.body!=nullptr && rayTestSingle(rayFromTrans, rayToTrans, *i.body, callback))
        return i.body;
    return nullptr;
    }

  bool rayTestSingle(const btTransform& s,
                     const btTransform& e, NpcBody& npc,
                     btCollisionWorld::RayResultCallback& callback){
    if(!npc.enable)
      return false;
    wrld.world->rayTestSingle(s, e, &npc,
                              npc.getCollisionShape(),
                              npc.getWorldTransform(),
                              callback);
    return callback.hasHit();
    }

  bool hasCollision(const DynamicWorld::Item& obj,std::array<float,3>& normal){
    static bool disable=false;
    if(disable)
      return false;

    const NpcBody* pn = dynamic_cast<const NpcBody*>(obj.obj);
    if(pn==nullptr)
      return false;
    const NpcBody& n = *pn;

    if(srt){
      if(hasCollision(n,frozen,normal,true))
        return true;
      return hasCollision(n,body,normal,false);
      } else {
      if(hasCollision(n,body,normal,false))
        return true;
      //adjustSort();
      return hasCollision(n,frozen,normal,false);
      }
    }

  bool hasCollision(const NpcBody& n,const std::vector<Record>& arr,std::array<float,3>& normal, bool sorted) {
    auto l = arr.begin();
    auto r = arr.end();

    if(sorted) {
      const float dX = maxR+n.r;
      l = std::lower_bound(arr.begin(),arr.end(),n.pos[0]-dX,[](const Record& b,float x){ return b.x<x; });
      r = std::upper_bound(arr.begin(),arr.end(),n.pos[0]+dX,[](float x,const Record& b){ return x<b.x; });
      }

    const auto dist = std::distance(l,r);
    if(dist<=1)
      return false;

    bool ret=false;
    for(;l!=r;++l){
      auto& v = (*l);
      if(v.body!=nullptr && v.body->enable && hasCollision(n,*v.body,normal))
        ret = true;
      }
    return ret;
    }

  bool hasCollision(const NpcBody& a,const NpcBody& b,std::array<float,3>& normal){
    if(&a==&b)
      return false;
    auto dx = a.pos[0]-b.pos[0], dy = a.pos[1]-b.pos[1], dz = a.pos[2]-b.pos[2];
    auto r  = a.r+b.r;

    if(dx*dx+dz*dz>r*r)
      return false;
    if(dy>b.h || dy<-a.h)
      return false;

    normal[0] += dx;
    normal[1] += dy;
    normal[2] += dz;
    return true;
    }

  void adjustSort() {
    srt=true;
    std::sort(frozen.begin(),frozen.end(),[](Record& a,Record& b){
      return a.x < b.x;
      });
    }

  void updateAabbs() {
    for(size_t i=0;i<body.size();) {
      if(body[i].body->lastMove!=tick){
        auto b = body[i];
        body[i]=body.back();
        body.pop_back();

        b.body->frozen=true;
        b.x = b.body->pos[0];
        frozen.push_back(b);
        } else {
        ++i;
        }
      }

    for(size_t i=0;i<frozen.size();) {
      if(frozen[i].body==nullptr) {
        frozen[i]=frozen.back();
        frozen.pop_back();
        }
      else if(frozen[i].body->lastMove==tick){
        frozen[i].body->frozen=false;
        body.push_back(frozen[i]);
        frozen[i]=frozen.back();
        frozen.pop_back();
        }
      else {
        ++i;
        }
      }

    adjustSort();
    tick++;
    }

  DynamicWorld&         wrld;
  std::vector<Record>   body, frozen;
  bool                  srt=false;
  uint64_t              tick=0;
  float                 maxR=0;
  };

struct DynamicWorld::BulletsList final {
  BulletsList(DynamicWorld& wrld):wrld(wrld){
    }

  BulletBody* add(BulletCallback* cb) {
    BulletBody b(&wrld,cb);
    body.push_front(std::move(b));
    return &body.front();
    }

  void del(BulletBody* b) {
    for(auto i=body.begin(), e=body.end();i!=e;++i){
      if(&(*i)==b) {
        body.erase(i);
        return;
        }
      }
    }

  void tick(uint64_t dt) {
    for(auto& i:body) {
      wrld.moveBullet(i,i.dir[0],i.dir[1],i.dir[2],dt);
      if(i.cb!=nullptr)
        i.cb->onMove();
      }
    }

  void onMoveNpc(NpcBody& npc, NpcBodyList& list){
    for(auto& i:body) {
      btVector3 s = {i.lastPos[0],i.lastPos[1],i.lastPos[2]};
      btVector3 e = {i.pos[0],i.pos[1],i.pos[2]};

      if(i.cb!=nullptr && list.rayTest(npc,s,e)) {
        i.cb->onCollide(*npc.getNpc());
        i.cb->onStop();
        }
      }
    }

  std::list<BulletBody> body;
  DynamicWorld&         wrld;
  };

DynamicWorld::DynamicWorld(World&,const PackedMesh& pkg) {
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

  landMesh .reset(new PhysicMesh(pkg.vertices));
  waterMesh.reset(new PhysicMesh(pkg.vertices));

  for(auto& i:pkg.subMeshes)
    if(!i.material.noCollDet && i.indices.size()>0) {
      if(i.material.matGroup==ZenLoad::MaterialGroup::WATER)
        waterMesh->addIndex(i.indices,i.material.matGroup); else
        landMesh ->addIndex(i.indices,i.material.matGroup);
      }

  landShape.reset(new btMultimaterialTriangleMeshShape(landMesh.get(),landMesh->useQuantization(),true));
  landBody = landObj();

  waterShape.reset(new btMultimaterialTriangleMeshShape(waterMesh.get(),waterMesh->useQuantization(),true));
  waterBody = waterObj();

  world->addCollisionObject(landBody.get());
  world->addCollisionObject(waterBody.get());

  world->setForceUpdateAllAabbs(false);

  npcList.reset(new NpcBodyList(*this));
  bulletList.reset(new BulletsList(*this));
  }

DynamicWorld::~DynamicWorld(){
  world->removeCollisionObject(waterBody.get());
  world->removeCollisionObject(landBody .get());
  }

std::array<float,3> DynamicWorld::landNormal(float x, float y, float z) const {
  struct rCallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;

    Category colCat=Category::C_Null;

    rCallBack(const btVector3& rayFromWorld, const btVector3& rayToWorld)
      :ClosestRayResultCallback(rayFromWorld,rayToWorld){
      m_collisionFilterMask = btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::StaticFilter;
      }

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      auto obj = rayResult.m_collisionObject;
      colCat = Category(obj->getUserIndex());
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
      int32_t uid = reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject)->getUserIndex();
      if(uid==C_Landscape || uid==C_Object)
        return ClosestRayResultCallback::needsCollision(proxy0);
      return false;
      }
    };

  btVector3 s(x,y+50,z), e(x,y-worldHeight,z);
  rCallBack callback{s,e};

  rayTest(s,e,callback);

  if(callback.hasHit() && callback.colCat==DynamicWorld::C_Landscape)
    return {{callback.m_hitNormalWorld.x(),callback.m_hitNormalWorld.y(),callback.m_hitNormalWorld.z()}};
  return {0,1,0};
  }

DynamicWorld::RayResult DynamicWorld::dropRay(float x, float y, float z) const {
  RayResult rayDrop = ray(x,y+ghostPadding,z, x,y-worldHeight,z);
  return rayDrop;
  }

DynamicWorld::RayResult DynamicWorld::waterRay(float x, float y, float z) const {
  RayResult rayDrop = implWaterRay(x,y,z, x,y+worldHeight,z);
  return rayDrop;
  }

DynamicWorld::RayResult DynamicWorld::implWaterRay(float x0, float y0, float z0, float x1, float y1, float z1) const {
  struct CallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;
    uint8_t  matId  = 0;
    Category colCat = C_Water;

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  btVector3 s(x0,y0,z0), e(x1,y1,z1);
  CallBack callback{s,e};
  //callback.m_flags = btTriangleRaycastCallback::kF_KeepUnflippedNormal | btTriangleRaycastCallback::kF_FilterBackfaces;

  btTransform rayFromTrans,rayToTrans;
  rayFromTrans.setIdentity();
  rayFromTrans.setOrigin(s);
  rayToTrans.setIdentity();
  rayToTrans.setOrigin(e);
  world->rayTestSingle(rayFromTrans, rayToTrans, waterBody.get(),
                       waterBody->getCollisionShape(),
                       waterBody->getWorldTransform(),
                       callback);

  if(callback.hasHit()){
    x1 = callback.m_hitPointWorld.x();
    y1 = callback.m_hitPointWorld.y();
    z1 = callback.m_hitPointWorld.z();
    } else {
    y1 = y0-worldHeight;
    }
  return RayResult{{x1,y1,z1},{0,1,0},callback.matId,callback.colCat,callback.hasHit()};
  }

DynamicWorld::RayResult DynamicWorld::ray(float x0, float y0, float z0, float x1, float y1, float z1) const {
  struct CallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;
    uint8_t  matId  = 0;
    Category colCat = C_Null;

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
      auto obj=reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject);
      if(obj->getUserIndex()==C_Landscape || obj->getUserIndex()==C_Object)
        return ClosestRayResultCallback::needsCollision(proxy0);
      return false;
      }

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      auto shape = rayResult.m_collisionObject->getCollisionShape();
      if(shape) {
        auto s  = reinterpret_cast<const btMultimaterialTriangleMeshShape*>(shape);
        auto mt = reinterpret_cast<const PhysicMesh*>(s->getMeshInterface());

        size_t id = size_t(rayResult.m_localShapeInfo->m_shapePart);
        matId  = mt->getMaterialId(id);
        }
      colCat = Category(rayResult.m_collisionObject->getUserIndex());
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  btVector3 s(x0,y0,z0), e(x1,y1,z1);
  CallBack callback{s,e};
  callback.m_flags = btTriangleRaycastCallback::kF_KeepUnflippedNormal | btTriangleRaycastCallback::kF_FilterBackfaces;

  rayTest(s,e,callback);

  float nx=0,ny=1,nz=0;
  if(callback.hasHit()){
    x1 = callback.m_hitPointWorld.x();
    y1 = callback.m_hitPointWorld.y();
    z1 = callback.m_hitPointWorld.z();
    if(callback.colCat==DynamicWorld::C_Landscape) {
      nx = callback.m_hitNormalWorld.x();
      ny = callback.m_hitNormalWorld.y();
      nz = callback.m_hitNormalWorld.z();
      }
    }
  return RayResult{{x1,y1,z1},{nx,ny,nz},
                   callback.matId,callback.colCat,callback.hasHit()};
  }

float DynamicWorld::soundOclusion(float x0, float y0, float z0, float x1, float y1, float z1) const {
  struct CallBack:btCollisionWorld::AllHitsRayResultCallback {
    using AllHitsRayResultCallback::AllHitsRayResultCallback;

    enum { FRAC_MAX=16 };
    uint32_t           cnt            = 0;
    float              frac[FRAC_MAX] = {};

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
      auto obj=reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject);
      int id = obj->getUserIndex();
      if(id==C_Landscape || id==C_Water || id==C_Object)
        return AllHitsRayResultCallback::needsCollision(proxy0);
      return false;
      }

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      if(cnt<FRAC_MAX)
        frac[cnt] = rayResult.m_hitFraction;
      cnt++;
      return AllHitsRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  btVector3 s(x0,y0,z0), e(x1,y1,z1);
  CallBack callback{s,e};
  callback.m_flags = btTriangleRaycastCallback::kF_KeepUnflippedNormal;

  rayTest(s,e,callback);
  if(callback.cnt<2)
    return 0;

  if(callback.cnt>=CallBack::FRAC_MAX)
    return 1;

  float fr=0;
  std::sort(callback.frac,callback.frac+callback.cnt);
  for(size_t i=1;i<callback.cnt;i+=2) {
    fr += (callback.frac[i]-callback.frac[i-1]);
    }

  float tlen = (s-e).length();
  // let's say: 1.5 meter wall blocks sound completly :)
  return (tlen*fr)/150.f;
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

std::unique_ptr<btRigidBody> DynamicWorld::waterObj() {
  btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
        0,                  // mass, in kg. 0 -> Static object, will never move.
        nullptr,
        waterShape.get(),
        btVector3(0,0,0)
        );
  std::unique_ptr<btRigidBody> obj(new btRigidBody(rigidBodyCI));
  obj->setUserIndex(C_Water);
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
  NpcBody*          obj   = new NpcBody(shape);

  btTransform trans;
  trans.setIdentity();
  obj->setWorldTransform(trans);
  obj->setUserIndex(C_Ghost);
  obj->setFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
  obj->setCollisionFlags(btCollisionObject::CO_RIGID_BODY);

  //world->addCollisionObject(obj);
  npcList->add(obj);
  npcList->resize(*obj,height,dx,dz);
  return Item(this,obj,height,dim*0.5f);
  }

DynamicWorld::StaticItem DynamicWorld::staticObj(const PhysicMeshShape *shape, const Tempest::Matrix4x4 &m) {
  if(shape==nullptr)
    return StaticItem();

  btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
        0,                  // mass, in kg. 0 -> Static object, will never move.
        nullptr,
        &shape->shape,
        btVector3(0,0,0)
        );
  std::unique_ptr<btRigidBody> obj(new btRigidBody(rigidBodyCI));
  obj->setUserIndex(C_Object);
  obj->setFlags(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_NO_CONTACT_RESPONSE);
  obj->setCollisionFlags(btCollisionObject::CO_RIGID_BODY);

  btTransform trans;
  trans.setFromOpenGLMatrix(m.data());
  obj->setWorldTransform(trans);

  world->addCollisionObject(obj.get());
  return StaticItem(this,obj.release());
  }

DynamicWorld::BulletBody* DynamicWorld::bulletObj(BulletCallback* cb) {
  return bulletList->add(cb);
  }

void DynamicWorld::moveBullet(BulletBody &b, float dx, float dy, float dz, uint64_t dt) {
  float k  = float(dt)/1000.f;
  const bool isSpell = b.isSpell();

  auto  p  = b.pos;
  float x0 = p[0];
  float y0 = p[1];
  float z0 = p[2];
  float x1 = x0+dx*k;
  float y1 = y0+dy*k - (isSpell ? 0 : gravity*k*k);
  float z1 = z0+dz*k;

  struct CallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;
    uint8_t  matId = ZenLoad::NUM_MAT_GROUPS;

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
      auto obj=reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject);
      if(obj->getUserIndex()==C_Landscape || obj->getUserIndex()==C_Object)
        return ClosestRayResultCallback::needsCollision(proxy0);
      return false;
      }

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      auto shape = rayResult.m_collisionObject->getCollisionShape();
      if(shape) {
        auto s  = reinterpret_cast<const btMultimaterialTriangleMeshShape*>(shape);
        auto mt = reinterpret_cast<const PhysicMesh*>(s->getMeshInterface());

        size_t id = size_t(rayResult.m_localShapeInfo->m_shapePart);
        matId  = mt->getMaterialId(id);
        }
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  btVector3 s(x0,y0,z0), e(x1,y1,z1);
  CallBack callback{s,e};
  callback.m_flags = btTriangleRaycastCallback::kF_KeepUnflippedNormal | btTriangleRaycastCallback::kF_FilterBackfaces;

  if(auto ptr = npcList->rayTest(s,e)) {
    if(b.cb!=nullptr) {
      b.cb->onCollide(*ptr->getNpc());
      b.cb->onStop();
      }
    return;
    }
  rayTest(s,e,callback);

  if(callback.matId<ZenLoad::NUM_MAT_GROUPS) {
    if( isSpell ){
      if(b.cb!=nullptr) {
        b.cb->onCollide(callback.matId);
        b.cb->onStop();
        }
      } else {
      if(callback.matId==ZenLoad::MaterialGroup::METAL ||
         callback.matId==ZenLoad::MaterialGroup::STONE) {
        auto d = b.dir;
        btVector3 m = {d[0],d[1],d[2]};
        btVector3 n = callback.m_hitNormalWorld;

        n.normalize();
        const float l = b.speed();
        m/=l;

        btVector3 dir = m - 2*m.dot(n)*n;
        dir*=(l*0.5f); //slow-down

        float a = callback.m_closestHitFraction;
        b.move(x0+(x1-x0)*a,y0+(y1-y0)*a,z0+(z1-z0)*a);
        if(l*a>10.f) {
          b.setDirection(dir.x(),dir.y(),dir.z());
          b.addPathLen(l*a);
          }
        } else {
        float a = callback.m_closestHitFraction;
        b.move(x0+(x1-x0)*a,y0+(y1-y0)*a,z0+(z1-z0)*a);
        }
      if(b.cb!=nullptr) {
        b.cb->onCollide(callback.matId);
        b.cb->onStop();
        }
      }
    } else {
    const float l = b.speed();
    auto d = b.direction();
    if(!isSpell)
      d[1] -= gravity*k;

    b.move(x1,y1,z1);
    b.setDirection(d[0],d[1],d[2]);
    b.addPathLen(l*k);
    }
  }

void DynamicWorld::tick(uint64_t dt) {
  npcList->updateAabbs();
  bulletList->tick(dt);
  }

void DynamicWorld::updateSingleAabb(btCollisionObject *obj) {
  world->updateSingleAabb(obj);
  }

void DynamicWorld::deleteObj(NpcBody *obj) {
  if(!obj)
    return;

  if(!npcList->del(obj))
    world->removeCollisionObject(obj);
  delete obj;
  }

void DynamicWorld::deleteObj(BulletBody* obj) {
  bulletList->del(obj);
  }

void DynamicWorld::deleteObj(btCollisionObject *obj) {
  if(!obj)
    return;

  if(!npcList->del(obj))
    world->removeCollisionObject(obj);
  delete obj;
  }

bool DynamicWorld::hasCollision(const Item& it,std::array<float,3>& normal) {
  if(npcList->hasCollision(it,normal)){
    float l = std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
    normal[0]/=l;
    normal[1]/=l;
    normal[2]/=l;
    return true;
    }

  struct rCallBack : public btCollisionWorld::ContactResultCallback {
    int                 count=0;
    std::array<float,3> norm={};
    btCollisionObject*  src=nullptr;

    explicit rCallBack(btCollisionObject* src):src(src){
      m_collisionFilterMask = btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::StaticFilter;
      }

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
      auto obj=reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject);
      if(obj->getUserIndex()!=C_Water)
        return ContactResultCallback::needsCollision(proxy0);
      return false;
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

template<class RayResultCallback>
void DynamicWorld::rayTest(const btVector3 &s,
                           const btVector3 &e,
                           RayResultCallback &callback) const {
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
    owner->npcList->move(*obj,x,y,z);
    }
  }

void DynamicWorld::Item::implSetPosition(float x, float y, float z) {
  btTransform trans;
  trans.setIdentity();
  trans.setOrigin(btVector3(x,y+(height-r-ghostPadding)*0.5f+r+ghostPadding,z));
  obj->setWorldTransform(trans);

  if(auto npc = dynamic_cast<NpcBody*>(obj)) {
    owner->npcList->move(*npc,x,y,z);
    owner->bulletList->onMoveNpc(*npc,*owner->npcList);
    }
  }

void DynamicWorld::Item::setEnable(bool e) {
  if(obj)
    obj->enable = e;
  }

void DynamicWorld::Item::setUserPointer(void *p) {
  obj->setUserPointer(p);
  }

float DynamicWorld::Item::centerY() const {
  if(obj) {
    const btTransform& tr = obj->getWorldTransform();
    return tr.getOrigin().y();
    }
  return 0;
  }

bool DynamicWorld::Item::testMove(const std::array<float,3> &pos) {
  if(!obj)
    return false;
  std::array<float,3> tmp={};
  if(owner->hasCollision(*this,tmp))
    return true;
  auto tr = obj->pos;
  implSetPosition(pos[0],pos[1],pos[2]);
  const bool ret=owner->hasCollision(*this,tmp);
  owner->npcList->move(*obj,tr);
  return !ret;
  }

bool DynamicWorld::Item::testMove(const std::array<float,3> &pos,
                                  std::array<float,3>       &fallback,
                                  float speed) {
  fallback=pos;
  if(!obj)
    return false;

  std::array<float,3> norm={};
  auto                tr = obj->pos;
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
  owner->npcList->move(*obj,tr);
  return !ret;
  }

bool DynamicWorld::Item::tryMoveN(const std::array<float,3> &pos, std::array<float,3> &norm) {
  norm = {};

  if(!obj)
    return false;
  auto tr = obj->pos;
  if(owner->hasCollision(*this,norm)){
    setPosition(pos[0],pos[1],pos[2]);
    return true;
    }

  implSetPosition(pos[0],pos[1],pos[2]);
  const bool ret=owner->hasCollision(*this,norm);
  if(!ret) {
    owner->npcList->move(*obj,pos[0],pos[1],pos[2]);
    return true;
    }

  owner->npcList->move(*obj,tr);
  return false;
  }

bool DynamicWorld::Item::tryMove(const std::array<float,3> &pos, std::array<float,3> &fallback, float speed) {
  fallback=pos;
  if(!obj)
    return false;

  std::array<float,3> norm={};
  auto                tr = obj->pos;
  if(owner->hasCollision(*this,norm)){
    setPosition(pos[0],pos[1],pos[2]);
    return true;
    }

  implSetPosition(pos[0],pos[1],pos[2]);
  const bool ret=owner->hasCollision(*this,norm);
  if(!ret) {
    owner->npcList->move(*obj,pos[0],pos[1],pos[2]);
    return true;
    }
  if(speed!=0.f){
    fallback[0] = pos[0] + norm[0]*speed;
    fallback[1] = pos[1];// - norm[1]*speed;
    fallback[2] = pos[2] + norm[2]*speed;
    }
  owner->npcList->move(*obj,tr);
  return false;
  }

bool DynamicWorld::Item::hasCollision() const {
  if(!obj)
    return false;
  std::array<float,3> tmp;
  return owner->hasCollision(*this,tmp);
  }

void DynamicWorld::StaticItem::setObjMatrix(const Tempest::Matrix4x4 &m) {
  if(obj){
    btTransform trans;
    trans.setFromOpenGLMatrix(reinterpret_cast<const btScalar*>(&m));
    obj->setWorldTransform(trans);
    owner->updateSingleAabb(obj);
    }
  }

DynamicWorld::BulletBody::BulletBody(DynamicWorld* wrld, DynamicWorld::BulletCallback* cb)
  :owner(wrld), cb(cb) {
  }

DynamicWorld::BulletBody::BulletBody(DynamicWorld::BulletBody&& other)
  : pos(other.pos), lastPos(other.lastPos),
    dir(other.dir), dirL(other.dirL), totalL(other.totalL), spl(other.spl){
  std::swap(owner,other.owner);
  std::swap(cb,other.cb);
  }

void DynamicWorld::BulletBody::setSpellId(int s) {
  spl = s;
  }

void DynamicWorld::BulletBody::move(float x, float y, float z) {
  lastPos = pos;
  pos     = {x,y,z};
  }

void DynamicWorld::BulletBody::setPosition(float x, float y, float z) {
  lastPos = {x,y,z};
  pos     = {x,y,z};
  }

void DynamicWorld::BulletBody::setDirection(float x, float y, float z) {
  dir  = {x,y,z};
  dirL = std::sqrt(x*x + y*y + z*z);
  }

float DynamicWorld::BulletBody::pathLength() const {
  return totalL;
  }

void DynamicWorld::BulletBody::addPathLen(float v) {
  totalL += v;
  }

Tempest::Matrix4x4 DynamicWorld::BulletBody::matrix() const {
  const float dx = dir[0]/dirL;
  const float dy = dir[1]/dirL;
  const float dz = dir[2]/dirL;

  float a2  = std::asin(dy)*float(180/M_PI);
  float ang = std::atan2(dz,dx)*float(180/M_PI)+180.f;

  Tempest::Matrix4x4 mat;
  mat.identity();
  mat.translate(pos[0],pos[1],pos[2]);
  mat.rotateOY(-ang);
  mat.rotateOZ(-a2);
  return mat;
  }
