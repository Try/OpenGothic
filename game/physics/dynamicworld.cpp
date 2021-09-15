#include "dynamicworld.h"

#include "physics.h"

#include "collisionworld.h"
#include "physicmeshshape.h"
#include "physicvbo.h"
#include "graphics/mesh/skeleton.h"

#include <algorithm>
#include <cmath>

#include "graphics/mesh/submesh/packedmesh.h"
#include "world/objects/item.h"
#include "world/bullet.h"
#include "world/world.h"

const float DynamicWorld::ghostPadding=50-22.5f;
const float DynamicWorld::ghostHeight =140;
const float DynamicWorld::worldHeight =20000;

struct DynamicWorld::HumShape:btCapsuleShape {
  HumShape(btScalar radius, btScalar height):btCapsuleShape((height<=0.f ? 0.f : radius)*0.01f,height*0.01f) {}

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

struct DynamicWorld::NpcBody : btRigidBody {
  NpcBody(btCollisionShape* shape):btRigidBody(0,nullptr,shape){}
  ~NpcBody() {
    delete m_collisionShape;
    }

  Tempest::Vec3 pos={};
  float         r=0, h=0, rX=0, rZ=0;
  bool          enable=true;
  size_t        frozen=size_t(-1);
  uint64_t      lastMove=0;

  Npc* toNpc() {
    return reinterpret_cast<Npc*>(getUserPointer());
    }

  void setPosition(const Tempest::Vec3& p) {
    auto m = CollisionWorld::toMeters(p+Tempest::Vec3(0,(h-r-ghostPadding)*0.5f+r+ghostPadding,0));
    pos = p;
    btTransform trans;
    trans.setIdentity();
    trans.setOrigin(m);
    setWorldTransform(trans);
    }
  };

struct DynamicWorld::NpcBodyList final {
  struct Record final {
    NpcBody* body = nullptr;
    float    x    = 0.f;
    };

  NpcBodyList(DynamicWorld& wrld):wrld(wrld){
    body  .reserve(1024);
    frozen.reserve(1024);
    }

  NpcBody* create(const ZMath::float3 &min, const ZMath::float3 &max) {
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

    add(obj);
    resize(*obj,height,dx,dz);
    return obj;
    }

  void add(NpcBody* b){
    Record r;
    r.body = b;
    r.x    = b->pos.x;
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

  bool delMisordered(NpcBody* b,std::vector<Record>& arr){
    auto&       fr = arr[b->frozen];
    const float x  = fr.x;
    if((b->frozen==0 || arr[b->frozen-1].x<x) &&
       (b->frozen+1==arr.size() || x<arr[b->frozen+1].x)) {
      fr.x = fr.body->pos.x;
      return false;
      } else {
      fr.body = nullptr;
      return true;
      }
    }

  void resize(NpcBody& n, float h, float dx, float dz){
    n.rX = dx;
    n.rZ = dz;

    //n.r = std::max(dx,dz)*0.5f;
    n.r = (dx+dz)*0.25f;
    n.h = h;

    maxR = std::max(maxR,n.r);
    }

  void onMove(NpcBody& n){
    if(n.frozen!=size_t(-1)) {
      if(delMisordered(&n,frozen)){
        n.lastMove = tick;
        n.frozen   = size_t(-1);

        Record r;
        r.body = &n;
        body.push_back(r);
        }
      } else {
      n.lastMove = tick;
      }
    }

  bool rayTest(NpcBody& npc, const Tempest::Vec3& s, const Tempest::Vec3& e, float extR) {
    if(!npc.enable)
      return false;
    auto  ln   = e       - s;
    auto  at   = npc.pos - s;

    float lenL = ln.length();
    float lenA = at.length();

    float dot  = Tempest::Vec3::dotProduct(ln,at);
    float div  = (lenL*lenA);
    float proj = dot/(div<=0 ? 1.f : div);
    proj = std::max(0.f,std::min(proj,1.f));

    auto  nr   = ln*proj + s;
    auto  dp   = nr      - npc.pos;
    float R    = npc.r + extR;
    if(dp.x*dp.x+dp.z*dp.z > R*R)
      return false;
    if(dp.y<0 || npc.h<dp.y)
      return false;
    return true;
    }

  NpcBody* rayTest(const Tempest::Vec3& s, const Tempest::Vec3& e, float extR) {
    for(auto i:body)
      if(rayTest(*i.body, s, e, extR))
        return i.body;
    for(auto i:frozen)
      if(i.body!=nullptr && rayTest(*i.body, s, e, extR))
        return i.body;
    return nullptr;
    }

  bool hasCollision(const DynamicWorld::NpcItem& obj,Tempest::Vec3& normal) {
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

  bool hasCollision(const NpcBody& n,const std::vector<Record>& arr,Tempest::Vec3& normal, bool sorted) {
    auto l = arr.begin();
    auto r = arr.end();

    if(sorted) {
      const float dX = maxR+n.r;
      l = std::lower_bound(arr.begin(),arr.end(),n.pos.x-dX,[](const Record& b,float x){ return b.x<x; });
      r = std::upper_bound(arr.begin(),arr.end(),n.pos.x+dX,[](float x,const Record& b){ return x<b.x; });
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

  bool hasCollision(const NpcBody& a, const NpcBody& b, Tempest::Vec3& normal){
    if(&a==&b)
      return false;
    auto dx = a.pos.x-b.pos.x, dy = a.pos.y-b.pos.y, dz = a.pos.z-b.pos.z;
    auto r  = a.r+b.r;

    if(dx*dx+dz*dz>r*r)
      return false;
    if(dy>b.h || dy<-a.h)
      return false;

    normal.x += dx;
    normal.y += dy;
    normal.z += dz;
    return true;
    }

  void adjustSort() {
    srt=true;
    std::sort(frozen.begin(),frozen.end(),[](Record& a,Record& b){
      return a.x < b.x;
      });
    for(size_t i=0; i<frozen.size(); ++i)
      frozen[i].body->frozen = i;
    }

  void tickAabbs() {
    for(size_t i=0;i<body.size();) {
      if(body[i].body->lastMove!=tick){
        auto b = body[i];
        body[i]=body.back();
        body.pop_back();

        b.x = b.body->pos.x;
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
        frozen[i].body->frozen=size_t(-1);
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
      wrld.moveBullet(i,i.dir,dt);
      if(i.cb!=nullptr)
        i.cb->onMove();
      }
    }

  void onMoveNpc(NpcBody& npc, NpcBodyList& list){
    for(auto& i:body) {
      if(i.cb!=nullptr && list.rayTest(npc,i.lastPos,i.pos,i.tgRange)) {
        i.cb->onCollide(*npc.toNpc());
        i.cb->onStop();
        }
      }
    }

  std::list<BulletBody> body;
  DynamicWorld&         wrld;
  };

struct DynamicWorld::BBoxList final {
  BBoxList(DynamicWorld& wrld):wrld(wrld){
    }

  void add(BBoxBody* b) {
    body.emplace_back(b);
    }

  void del(BBoxBody* b) {
    for(auto i=body.begin(), e=body.end();i!=e;++i){
      if(*i==b) {
        body.erase(i);
        return;
        }
      }
    }

  BBoxBody* rayTest(const btVector3& s, const btVector3& e) {
    struct CallBack:btCollisionWorld::ClosestRayResultCallback {
      using ClosestRayResultCallback::ClosestRayResultCallback;
      };
    CallBack callback{s,e};

    btTransform rayFromTrans,rayToTrans;
    rayFromTrans.setIdentity();
    rayFromTrans.setOrigin(s);
    rayToTrans.setIdentity();
    rayToTrans.setOrigin(e);
    for(auto& i:body)
      if(rayTestSingle(rayFromTrans, rayToTrans, *i, callback))
        return i;
    return nullptr;
    }

  bool rayTestSingle(const btTransform& s,
                     const btTransform& e, BBoxBody& npc,
                     btCollisionWorld::RayResultCallback& callback){
    wrld.world->rayTestSingle(s, e, npc.obj,
                              npc.shape,
                              npc.obj->getWorldTransform(),
                              callback);
    return callback.hasHit();
    }

  std::vector<BBoxBody*> body;
  DynamicWorld&          wrld;
  };

DynamicWorld::DynamicWorld(World& owner,const ZenLoad::zCMesh& worldMesh) {
  //solver.reset(new btSequentialImpulseConstraintSolver());
  world.reset(new CollisionWorld());

  PackedMesh pkg(worldMesh,PackedMesh::PK_PhysicZoned);
  sectors.resize(pkg.subMeshes.size());
  for(size_t i=0;i<sectors.size();++i)
    sectors[i] = pkg.subMeshes[i].material.matName;

  landVbo.resize(pkg.vertices.size());
  for(size_t i=0;i<pkg.vertices.size();++i) {
    auto p = CollisionWorld::toMeters(pkg.vertices[i].Position);
    landVbo[i] = p;
    }

  landMesh .reset(new PhysicVbo(&landVbo));
  waterMesh.reset(new PhysicVbo(&landVbo));

  for(size_t i=0;i<pkg.subMeshes.size();++i) {
    auto& sm = pkg.subMeshes[i];
    if(!sm.material.noCollDet && sm.indices.size()>0) {
      if(sm.material.matGroup==ZenLoad::MaterialGroup::WATER) {
        waterMesh->addIndex(std::move(sm.indices),sm.material.matGroup);
        } else {
        landMesh ->addIndex(std::move(sm.indices),sm.material.matGroup,sectors[i].c_str());
        }
      }
    }

  btVector3 bbox[2] = {};
  if(!landMesh->isEmpty()) {
    Tempest::Matrix4x4 mt;
    mt.identity();
    landShape.reset(new btMultimaterialTriangleMeshShape(landMesh.get(),landMesh->useQuantization(),true));
    landBody = world->addCollisionBody(*landShape,mt,DynamicWorld::materialFriction(ZenLoad::NUM_MAT_GROUPS));
    landBody->setUserIndex(C_Landscape);

    btVector3 b[2] = {};
    landBody->getAabb(b[0],b[1]);
    bbox[0].setMin(b[0]);
    bbox[1].setMax(b[1]);
    }

  if(!waterMesh->isEmpty()) {
    Tempest::Matrix4x4 mt;
    mt.identity();
    waterShape.reset(new btMultimaterialTriangleMeshShape(waterMesh.get(),waterMesh->useQuantization(),true));
    waterBody = world->addCollisionBody(*waterShape,mt,0);
    waterBody->setUserIndex(C_Water);
    waterBody->setFlags(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_NO_CONTACT_RESPONSE);
    waterBody->setCollisionFlags(btCollisionObject::CO_HF_FLUID);

    btVector3 b[2] = {};
    landBody->getAabb(b[0],b[1]);
    bbox[0].setMin(b[0]);
    bbox[1].setMax(b[1]);
    }

  world->setBBox(bbox[0],bbox[1]);
  npcList   .reset(new NpcBodyList(*this));
  bulletList.reset(new BulletsList(*this));
  bboxList  .reset(new BBoxList   (*this));

  world->setItemHitCallback([&](::Item& itm, ZenLoad::MaterialGroup mat, float impulse, float mass) {
    auto  snd = owner.addLandHitEffect(ItemMaterial(itm.handle().material),mat,itm.transform());
    float v   = impulse/mass;
    float vol = snd.volume()*std::min(v/10.f,1.f);
    snd.setVolume(vol);
    snd.play();
    });
  }

DynamicWorld::~DynamicWorld(){
  }

DynamicWorld::RayLandResult DynamicWorld::landRay(const Tempest::Vec3& from, float maxDy) const {
  world->updateAabbs();
  if(maxDy==0)
    maxDy = worldHeight;
  return ray(Tempest::Vec3(from.x,from.y+ghostPadding,from.z), Tempest::Vec3(from.x,from.y-maxDy,from.z));
  }

DynamicWorld::RayWaterResult DynamicWorld::waterRay(const Tempest::Vec3& from) const {
  world->updateAabbs();
  return implWaterRay(from, Tempest::Vec3(from.x,from.y+worldHeight,from.z));
  }

DynamicWorld::RayWaterResult DynamicWorld::implWaterRay(const Tempest::Vec3& from, const Tempest::Vec3& to) const {
  struct CallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  CallBack callback{CollisionWorld::toMeters(from), CollisionWorld::toMeters(to)};
  //callback.m_flags = btTriangleRaycastCallback::kF_KeepUnflippedNormal | btTriangleRaycastCallback::kF_FilterBackfaces;

  if(waterBody!=nullptr) {
    btTransform rayFromTrans,rayToTrans;
    rayFromTrans.setIdentity();
    rayFromTrans.setOrigin(callback.m_rayFromWorld);
    rayToTrans.setIdentity();
    rayToTrans.setOrigin(callback.m_rayToWorld);
    world->rayTestSingle(rayFromTrans, rayToTrans,
                         waterBody.get(),
                         waterBody->getCollisionShape(),
                         waterBody->getWorldTransform(),
                         callback);
    }

  RayWaterResult ret;
  if(callback.hasHit()) {
    float waterY = callback.m_hitPointWorld.y()*100.f;
    auto  cave   = ray(from,Tempest::Vec3(to.x,waterY,to.z));
    if(cave.hasCol && cave.v.y<waterY) {
      ret.wdepth = from.y-worldHeight;
      ret.hasCol = false;
      } else {
      ret.wdepth = waterY;
      ret.hasCol = true;
      }
    return ret;
    }

  ret.wdepth = from.y-worldHeight;
  ret.hasCol = false;
  return ret;
  }

DynamicWorld::RayLandResult DynamicWorld::ray(const Tempest::Vec3& from, const Tempest::Vec3& to) const {
  struct CallBack:btCollisionWorld::ClosestRayResultCallback {
    using ClosestRayResultCallback::ClosestRayResultCallback;
    uint8_t     matId  = 0;
    const char* sector = nullptr;
    Category    colCat = C_Null;

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
      auto obj=reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject);
      if(obj->getUserIndex()==C_Landscape || obj->getUserIndex()==C_Object)
        return ClosestRayResultCallback::needsCollision(proxy0);
      return false;
      }

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
      auto shape = rayResult.m_collisionObject->getCollisionShape();
      if(shape!=nullptr) {
        auto s  = reinterpret_cast<const btMultimaterialTriangleMeshShape*>(shape);
        auto mt = reinterpret_cast<const PhysicVbo*>(s->getMeshInterface());

        size_t id = size_t(rayResult.m_localShapeInfo->m_shapePart);
        matId  = mt->materialId(id);
        sector = mt->sectorName(id);
        }
      colCat = Category(rayResult.m_collisionObject->getUserIndex());
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  CallBack callback{CollisionWorld::toMeters(from), CollisionWorld::toMeters(to)};
  callback.m_flags = btTriangleRaycastCallback::kF_KeepUnflippedNormal | btTriangleRaycastCallback::kF_FilterBackfaces;

  world->rayCast(from,to,callback);

  Tempest::Vec3 hitPos = to, hitNorm;
  if(callback.hasHit()){
    hitPos = CollisionWorld::toCentimeters(callback.m_hitPointWorld);
    if(callback.colCat==DynamicWorld::C_Landscape) {
      hitNorm.x = callback.m_hitNormalWorld.x();
      hitNorm.y = callback.m_hitNormalWorld.y();
      hitNorm.z = callback.m_hitNormalWorld.z();
      }
    }

  RayLandResult ret;
  ret.v      = hitPos;
  ret.n      = hitNorm;
  ret.mat    = callback.matId;
  ret.hasCol = callback.hasHit();
  ret.sector = callback.sector;
  return ret;
  }

float DynamicWorld::soundOclusion(const Tempest::Vec3& from, const Tempest::Vec3& to) const {
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

  CallBack callback(CollisionWorld::toMeters(from), CollisionWorld::toMeters(to));
  callback.m_flags = btTriangleRaycastCallback::kF_KeepUnflippedNormal;

  world->rayCast(from,to,callback);
  if(callback.cnt<2)
    return 0;

  if(callback.cnt>=CallBack::FRAC_MAX)
    return 1;

  float fr=0;
  std::sort(callback.frac,callback.frac+callback.cnt);
  for(size_t i=1;i<callback.cnt;i+=2) {
    fr += (callback.frac[i]-callback.frac[i-1]);
    }

  float tlen = (callback.m_rayFromWorld-callback.m_rayToWorld).length();
  // let's say: 1.5 meter wall blocks sound completly :)
  return (tlen*fr)/1.5f;
  }

DynamicWorld::NpcItem DynamicWorld::ghostObj(std::string_view visual) {
  ZMath::float3 min={0,0,0}, max={0,0,0};
  if(auto sk=Resources::loadSkeleton(visual)) {
    min = sk->bboxCol[0];
    max = sk->bboxCol[1];
    }
  auto  obj = npcList->create(min,max);
  float dim = std::max(obj->rX,obj->rZ);
  return NpcItem(this,obj,dim*0.5f);
  }

DynamicWorld::Item DynamicWorld::staticObj(const PhysicMeshShape *shape, const Tempest::Matrix4x4 &m) {
  if(shape==nullptr)
    return Item();
  return createObj(&shape->shape,false,m,0,shape->friction(),IT_Static);
  }

DynamicWorld::Item DynamicWorld::movableObj(const PhysicMeshShape* shape, const Tempest::Matrix4x4& m) {
  if(shape==nullptr)
    return Item();
  return createObj(&shape->shape,false,m,0,shape->friction(),IT_Movable);
  }

DynamicWorld::Item DynamicWorld::createObj(btCollisionShape* shape, bool ownShape, const Tempest::Matrix4x4& m, float mass, float friction, ItemType type) {
  std::unique_ptr<CollisionWorld::CollisionBody> obj;
  switch(type) {
    case IT_Movable:
    case IT_Static:
      obj = world->addCollisionBody(*shape,m,friction);
      obj->setUserIndex(C_Object);
      break;
    case IT_Dynamic:
      obj = world->addDynamicBody(*shape,m,friction,mass);
      obj->setUserIndex(C_Item);
      break;
    }
  return Item(this,obj.release(),ownShape ? shape : nullptr);
  }

DynamicWorld::Item DynamicWorld::dynamicObj(const Tempest::Matrix4x4& pos, const Bounds& b, ZenLoad::MaterialGroup mat) {
  btVector3 hExt = {b.bbox[1].x-b.bbox[0].x, b.bbox[1].y-b.bbox[0].y, b.bbox[1].z-b.bbox[0].z};
  hExt *= 0.01f;

  float density = DynamicWorld::materialDensity(mat);
  float mass    = density*(hExt[0])*(hExt[1])*(hExt[2]);
  //mass = 0.1f;
  for(int i=0;i<3;++i)
    hExt[i] = std::max(hExt[i]*0.5f,0.15f);

  std::unique_ptr<btCollisionShape> shape { new btBoxShape(hExt) };
  return createObj(shape.release(),true,pos,mass,materialFriction(mat),IT_Dynamic);
  }

DynamicWorld::BulletBody* DynamicWorld::bulletObj(BulletCallback* cb) {
  return bulletList->add(cb);
  }

DynamicWorld::BBoxBody DynamicWorld::bboxObj(BBoxCallback* cb, const ZMath::float3* bbox) {
  return BBoxBody(this,cb,bbox);
  }

DynamicWorld::BBoxBody DynamicWorld::bboxObj(BBoxCallback* cb, const Tempest::Vec3& pos, float R) {
  return BBoxBody(this,cb,pos,R);
  }

void DynamicWorld::moveBullet(BulletBody &b, const Tempest::Vec3& dir, uint64_t dt) {
  const float dtF     = float(dt);
  const bool  isSpell = b.isSpell();

  auto  pos = b.pos;
  auto  to  = pos + dir*dtF - Tempest::Vec3(0,(isSpell ? 0 : gravity*dtF*dtF),0);

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
        auto mt = reinterpret_cast<const PhysicVbo*>(s->getMeshInterface());

        size_t id = size_t(rayResult.m_localShapeInfo->m_shapePart);
        matId  = mt->materialId(id);
        }
      return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
      }
    };

  btVector3 s=CollisionWorld::toMeters(pos), e=CollisionWorld::toMeters(to);

  CallBack callback{s,e};
  callback.m_flags = btTriangleRaycastCallback::kF_KeepUnflippedNormal | btTriangleRaycastCallback::kF_FilterBackfaces;

  if(auto ptr = bboxList->rayTest(s,e)) {
    if(ptr->cb!=nullptr) {
      ptr->cb->onCollide(b);
      }
    }

  if(auto ptr = npcList->rayTest(pos,to,b.targetRange())) {
    if(b.cb!=nullptr) {
      b.cb->onCollide(*ptr->toNpc());
      b.cb->onStop();
      }
    return;
    }
  world->rayCast(pos, to, callback);

  if(callback.matId<ZenLoad::NUM_MAT_GROUPS) {
    if(isSpell){
      if(b.cb!=nullptr) {
        b.cb->onCollide(callback.matId);
        b.cb->onStop();
        }
      } else {
      if(callback.matId==ZenLoad::MaterialGroup::METAL ||
         callback.matId==ZenLoad::MaterialGroup::STONE) {
        auto d = b.dir;
        btVector3 m = {d.x,d.y,d.z};
        btVector3 n = callback.m_hitNormalWorld;

        n.normalize();
        const float l = b.speed();
        m/=l;

        btVector3 dir = m - 2*m.dot(n)*n;
        dir*=(l*0.5f); //slow-down

        float a = callback.m_closestHitFraction;
        b.move(pos + (to-pos)*a);
        if(l*a>0.1f) {
          b.setDirection({dir.x(),dir.y(),dir.z()});
          b.addPathLen(l*a);
          b.addHit();
          if(b.cb!=nullptr) {
            b.cb->onCollide(callback.matId);
            if(b.hitCount()>3)
              b.cb->onStop();
            }
          }
        } else {
        float a = callback.m_closestHitFraction;
        b.move(pos + (to-pos)*a);
        if(b.cb!=nullptr) {
          b.cb->onCollide(callback.matId);
          b.cb->onStop();
          }
        }
      }
    } else {
    const float l = b.speed();
    auto        d = b.direction();
    if(!isSpell)
      d.y -= (gravity)*dtF;

    b.move(to);
    b.setDirection(d);
    b.addPathLen(l*dtF);
    }
  }

void DynamicWorld::tick(uint64_t dt) {
  npcList   ->tickAabbs();
  bulletList->tick(dt);
  world     ->tick(dt);
  }

void DynamicWorld::deleteObj(BulletBody* obj) {
  bulletList->del(obj);
  }

float DynamicWorld::materialFriction(ZenLoad::MaterialGroup mat) {
  // https://www.thoughtspike.com/friction-coefficients-for-bullet-physics/
  switch(mat) {
    case ZenLoad::MaterialGroup::UNDEF:
      return 0.5f;
    case ZenLoad::MaterialGroup::METAL:
      return 1.1f;
    case ZenLoad::MaterialGroup::STONE:
      return 0.65f;
    case ZenLoad::MaterialGroup::WOOD:
      return 0.4f;
    case ZenLoad::MaterialGroup::EARTH:
      return 0.4f;
    case ZenLoad::MaterialGroup::WATER:
      return 0.01f;
    case ZenLoad::MaterialGroup::SNOW:
      return 0.2f;
    case ZenLoad::MaterialGroup::NUM_MAT_GROUPS:
      break;
    }
  return 0.75f;
  }

float DynamicWorld::materialDensity(ZenLoad::MaterialGroup mat) {
  switch(mat) {
    case ZenLoad::MaterialGroup::UNDEF:
      return 2000.0f;
    case ZenLoad::MaterialGroup::METAL:
      return 7800.f;
    case ZenLoad::MaterialGroup::STONE:
      return 2200.f;
    case ZenLoad::MaterialGroup::WOOD:
      return 700.f;
    case ZenLoad::MaterialGroup::EARTH:
      return 1500.f;
    case ZenLoad::MaterialGroup::WATER:
      return 1000.f;
    case ZenLoad::MaterialGroup::SNOW:
      return 1000.f;
    case ZenLoad::MaterialGroup::NUM_MAT_GROUPS:
      break;
    }
  return 2000.f;
  }

const char* DynamicWorld::validateSectorName(const char* name) const {
  return landMesh->validateSectorName(name);
  }

bool DynamicWorld::hasCollision(const NpcItem& it, CollisionTest& out) {
  if(npcList->hasCollision(it,out.normal)){
    out.normal /= out.normal.length();
    out.npcCol = true;
    return true;
    }
  return world->hasCollision(*it.obj,out.normal);
  }

DynamicWorld::NpcItem::~NpcItem() {
  if(owner==nullptr)
    return;
  owner->world->touchAabbs();
  if(!owner->npcList->del(obj))
    owner->world->removeCollisionObject(obj);
  }

void DynamicWorld::NpcItem::setPosition(const Tempest::Vec3& pos) {
  if(obj) {
    implSetPosition(pos);
    owner->npcList->onMove(*obj);
    owner->bulletList->onMoveNpc(*obj,*owner->npcList);
    }
  }

void DynamicWorld::NpcItem::implSetPosition(const Tempest::Vec3& pos) {
  obj->setPosition(pos);
  }

void DynamicWorld::NpcItem::setEnable(bool e) {
  if(obj==nullptr || obj->enable==e)
    return;
  obj->enable = e;
  owner->world->touchAabbs();
  }

void DynamicWorld::NpcItem::setUserPointer(void *p) {
  obj->setUserPointer(p);
  }

float DynamicWorld::NpcItem::centerY() const {
  if(obj) {
    const btTransform& tr = obj->getWorldTransform();
    return tr.getOrigin().y()*100.f;
    }
  return 0;
  }

const Tempest::Vec3& DynamicWorld::NpcItem::position() const {
  return obj->pos;
  }

bool DynamicWorld::NpcItem::testMove(const Tempest::Vec3& to, CollisionTest& out) {
  if(!obj)
    return false;
  return testMove(to,obj->pos,out);
  }

bool DynamicWorld::NpcItem::testMove(const Tempest::Vec3& to, const Tempest::Vec3& pos0, CollisionTest& out) {
  if(!obj)
    return false;
  auto prev = obj->pos;
  auto code = implTryMove(to,pos0,out);
  implSetPosition(prev);
  return code==MoveCode::MC_OK;
  }

DynamicWorld::MoveCode DynamicWorld::NpcItem::tryMove(const Tempest::Vec3& to, CollisionTest& out) {
  // 2-santimeters
  static const float eps = 2;

  if(!obj)
    return MoveCode::MC_Fail;

  auto dp = to - obj->pos;
  if(std::abs(dp.x)<eps && std::abs(dp.y)<eps && std::abs(dp.z)<eps) {
    // skip-move
    return MoveCode::MC_Skip;
    }

  auto code = implTryMove(to,obj->pos,out);
  switch(code) {
    case MoveCode::MC_Fail:
    case MoveCode::MC_Skip:
      return code;
    case MoveCode::MC_Partial:
      setPosition(out.partial);
      return MoveCode::MC_Partial;
    case MoveCode::MC_OK:
      owner->npcList->onMove(*obj);
      owner->bulletList->onMoveNpc(*obj,*owner->npcList);
      return MoveCode::MC_OK;
    }
  return code;
  }

DynamicWorld::MoveCode DynamicWorld::NpcItem::implTryMove(const Tempest::Vec3& to, const Tempest::Vec3& pos0, CollisionTest& out) {
  auto initial = pos0;
  auto r       = obj->r;
  int  count   = 1;
  auto dp      = to-initial;

  if((dp.x*dp.x+dp.z*dp.z)>r*r || dp.y>obj->h*0.5f) {
    const int countXZ = int(std::ceil(std::sqrt(dp.x*dp.x+dp.z*dp.z)/r));
    const int countY  = int(std::ceil(std::abs(dp.y)/(obj->h*0.5f)));

    count = std::max(countXZ,countY);
    }

  auto prev = initial;
  for(int i=1; i<=count; ++i) {
    auto pos = initial+(dp*float(i))/float(count);
    implSetPosition(pos);
    if(owner->hasCollision(*this,out)) {
      if(i>1) {
        // moved a bit
        out.partial = prev;
        return MoveCode::MC_Partial;
        }
      implSetPosition(initial);
      if(owner->hasCollision(*this,out)) {
        // was in collision from the start
        implSetPosition(to);
        return MoveCode::MC_OK;
        }
      return MoveCode::MC_Fail;
      }
    }

  return MoveCode::MC_OK;
  }

bool DynamicWorld::NpcItem::hasCollision() const {
  if(!obj)
    return false;
  CollisionTest info;
  return owner->hasCollision(*this,info);
  }

DynamicWorld::Item::~Item() {
  delete obj;
  delete shp;
  }

void DynamicWorld::Item::setObjMatrix(const Tempest::Matrix4x4 &m) {
  if(obj!=nullptr) {
    btTransform trans;
    trans.setFromOpenGLMatrix(reinterpret_cast<const btScalar*>(&m));
    trans.getOrigin()*=0.01f;
    if(obj->getWorldTransform()==trans)
      return;
    obj->setWorldTransform(trans);
    //owner->world->touchAabbs(); // TOO SLOW!
    owner->world->updateSingleAabb(obj);
    }
  }

void DynamicWorld::Item::setItem(::Item* it) {
  obj->setUserPointer(it);
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

void DynamicWorld::BulletBody::setTargetRange(float t) {
  tgRange = t;
  }

void DynamicWorld::BulletBody::move(const Tempest::Vec3& to) {
  lastPos = pos;
  pos     = to;
  }

void DynamicWorld::BulletBody::setPosition(const Tempest::Vec3& p) {
  lastPos = p;
  pos     = p;
  }

void DynamicWorld::BulletBody::setDirection(const Tempest::Vec3& d) {
  dir  = d;
  dirL = d.length();
  }

float DynamicWorld::BulletBody::pathLength() const {
  return totalL;
  }

void DynamicWorld::BulletBody::addPathLen(float v) {
  totalL += v;
  }

Tempest::Matrix4x4 DynamicWorld::BulletBody::matrix() const {
  const float dx = dir.x/dirL;
  const float dy = dir.y/dirL;
  const float dz = dir.z/dirL;

  float a2  = std::asin(dy)*float(180/M_PI);
  float ang = std::atan2(dz,dx)*float(180/M_PI)+180.f;

  Tempest::Matrix4x4 mat;
  mat.identity();
  mat.translate(pos.x,pos.y,pos.z);
  mat.rotateOY(-ang);
  mat.rotateOZ(-a2);
  return mat;
  }


DynamicWorld::BBoxBody::BBoxBody(DynamicWorld* ow, DynamicWorld::BBoxCallback* cb, const ZMath::float3* bbox)
  : owner(ow), cb(cb) {
  btVector3 hExt = CollisionWorld::toMeters(Tempest::Vec3{bbox[1].x-bbox[0].x, bbox[1].y-bbox[0].y, bbox[1].z-bbox[0].z})*0.5f;
  btVector3 pos  = CollisionWorld::toMeters(Tempest::Vec3{bbox[1].x+bbox[0].x, bbox[1].y+bbox[0].y, bbox[1].z+bbox[0].z})*0.5f;

  shape = new btBoxShape(hExt);
  obj   = new btRigidBody(btRigidBody::btRigidBodyConstructionInfo(0,nullptr,shape));

  btTransform trans;
  trans.setIdentity();
  trans.setOrigin(pos);

  obj->setWorldTransform(trans);
  obj->setUserIndex(C_Ghost);
  obj->setFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
  //obj->setCollisionFlags(btCollisionObject::CO_RIGID_BODY);

  owner->bboxList->add(this);
  }

DynamicWorld::BBoxBody::BBoxBody(DynamicWorld* ow, BBoxCallback* cb, const Tempest::Vec3& p, float R)
  : owner(ow), cb(cb) {
  btVector3 pos = CollisionWorld::toMeters(p);

  shape = new btCapsuleShape(R,4.f);
  obj   = new btRigidBody(btRigidBody::btRigidBodyConstructionInfo(0,nullptr,shape));

  btTransform trans;
  trans.setIdentity();
  trans.setOrigin(pos);

  obj->setWorldTransform(trans);
  obj->setUserIndex(C_Ghost);
  obj->setFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
  //obj->setCollisionFlags(btCollisionObject::CO_RIGID_BODY);

  owner->bboxList->add(this);
  }

DynamicWorld::BBoxBody::BBoxBody(BBoxBody&& other)
  :owner(other.owner),cb(other.cb),shape(other.shape),obj(other.obj) {
  other.owner = nullptr;
  other.cb    = nullptr;
  other.shape = nullptr;
  other.obj   = nullptr;

  if(owner!=nullptr) {
    owner->bboxList->del(&other);
    owner->bboxList->add(this);
    }
  }

DynamicWorld::BBoxBody& DynamicWorld::BBoxBody::operator=(BBoxBody&& other) {
  if(other.owner!=nullptr)
    other.owner->bboxList->del(&other);
  if(owner!=nullptr)
    owner->bboxList->del(this);

  std::swap(owner,other.owner);
  std::swap(cb,   other.cb);
  std::swap(shape,other.shape);
  std::swap(obj,  other.obj);

  if(other.owner!=nullptr)
    other.owner->bboxList->add(&other);
  if(owner!=nullptr)
    owner->bboxList->add(this);

  return *this;
  }

DynamicWorld::BBoxBody::~BBoxBody() {
  if(owner==nullptr)
    return;
  owner->bboxList->del(this);
  delete obj;
  delete shape;
  }
