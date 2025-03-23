#include "visualobjects.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/animmesh.h"
#include "gothic.h"

using namespace Tempest;

static RtScene::Category toRtCategory(DrawCommands::Type t) {
  switch (t) {
    case DrawCommands::Landscape: return RtScene::Landscape;
    case DrawCommands::Static:    return RtScene::Static;
    case DrawCommands::Movable:   return RtScene::Movable;
    case DrawCommands::Animated:  return RtScene::None;
    case DrawCommands::Pfx:       return RtScene::None;
    case DrawCommands::Morph:     return RtScene::None;
    }
  return RtScene::None;
  }


struct VisualObjects::InstanceDesc {
  void setPosition(const Tempest::Matrix4x4& m) {
    for(int i=0; i<4; ++i)
      for(int r=0; r<3; ++r)
        pos[i][r] = m.at(i,r);
    }
  float    pos[4][3] = {};
  float    fatness   = 0;
  uint32_t animPtr   = 0;
  uint32_t padd0     = {};
  uint32_t padd1     = {};
  };

struct VisualObjects::MorphDesc final {
  uint32_t indexOffset = 0;
  uint32_t sample0     = 0;
  uint32_t sample1     = 0;
  uint16_t alpha       = 0;
  uint16_t intensity   = 0;
  };

struct VisualObjects::MorphData {
  MorphDesc morph[Resources::MAX_MORPH_LAYERS];
  };


void VisualObjects::Item::setObjMatrix(const Tempest::Matrix4x4& pos) {
  if(owner!=nullptr) {
    auto& obj = owner->objects[id];
    if(obj.pos!=pos)
      owner->updateRtAs(id);
    obj.pos = pos;
    owner->updateInstance(id);
    }
  }

void VisualObjects::Item::setAsGhost(bool g) {
  if(owner!=nullptr) {
    owner->setAsGhost(id, g);
    }
  }

void VisualObjects::Item::setFatness(float f) {
  if(owner!=nullptr) {
    if(owner->objects[id].fatness == f)
      return;
    owner->objects[id].fatness = f;
    owner->updateInstance(id);
    }
  }

void VisualObjects::Item::setWind(zenkit::AnimationType m, float intensity) {
  if(owner==nullptr)
    return;

  if(intensity!=0 && m==zenkit::AnimationType::NONE)
    m = zenkit::AnimationType::WIND_ALT;

  if(intensity==0)
    m = zenkit::AnimationType::NONE;

  auto& obj = owner->objects[id];

  const auto prev = obj.wind;
  obj.wind          = m;
  obj.windIntensity = intensity;

  if(prev==m)
    return;

  if(prev!=zenkit::AnimationType::NONE)
    owner->objectsWind.erase(id);
  if(m!=zenkit::AnimationType::NONE)
    owner->objectsWind.insert(id);
  }

void VisualObjects::Item::startMMAnim(std::string_view anim, float intensity, uint64_t timeUntil) {
  if(owner!=nullptr)
    owner->startMMAnim(id, anim, intensity, timeUntil);
  }

const Material& VisualObjects::Item::material() const {
  if(owner==nullptr) {
    static Material m;
    return m;
    }
  auto& bx = *owner->objects[id].bucketId;
  return bx.mat;
  }

const Bounds& VisualObjects::Item::bounds() const {
  static Bounds dummy;
  if(owner==nullptr)
    return dummy;
  auto& bx = *owner->objects[id].bucketId;
  if(bx.staticMesh!=nullptr)
    return bx.staticMesh->bbox;
  if(bx.animMesh!=nullptr)
    return bx.animMesh->bbox;
  return dummy;
  }

Matrix4x4 VisualObjects::Item::position() const {
  if(owner!=nullptr)
    return owner->objects[id].pos;
  return Matrix4x4();
  }

const StaticMesh* VisualObjects::Item::mesh() const {
  if(owner==nullptr)
    return nullptr;
  auto& bx = *owner->objects[id].bucketId;
  return bx.staticMesh;
  }

std::pair<uint32_t, uint32_t> VisualObjects::Item::meshSlice() const {
  if(owner==nullptr)
    return std::pair<uint32_t, uint32_t>(0,0);
  auto& obj = owner->objects[id];
  return std::make_pair(obj.iboOff, obj.iboLen);
  }


VisualObjects::VisualObjects(const SceneGlobals& scene, const std::pair<Vec3, Vec3>& bbox)
    : scene(scene), drawCmd(*this, bucketsMem, clustersMem, scene) {
  objectsMorph.reserve(1024);
  }

VisualObjects::~VisualObjects() {
  }

void VisualObjects::updateInstance(size_t id, Matrix4x4* pos) {
  auto& obj = objects[id];
  if(obj.type==DrawCommands::Landscape)
    return;

  InstanceDesc d;
  d.setPosition(pos==nullptr ? obj.pos : *pos);
  d.animPtr = obj.animPtr;
  d.fatness = obj.fatness*0.5f;
  obj.objInstance.set(&d, 0, sizeof(d));

  auto cId  = obj.clusterId;
  auto npos = Vec3(obj.pos[3][0], obj.pos[3][1], obj.pos[3][2]);
  if(clustersMem[cId].pos != npos) {
    clustersMem[cId].pos = npos;
    clustersMem.markClusters(cId);
    }
  }

void VisualObjects::updateRtAs(size_t id) {
  auto& obj = objects[id];
  auto& mat = obj.bucketId->mat;

  bool useBlas = toRtCategory(obj.type)!=RtScene::None;
  if(mat.alpha==Material::Ghost)
    useBlas = false;
  if(mat.alpha!=Material::Solid && mat.alpha!=Material::AlphaTest && mat.alpha!=Material::Transparent)
    useBlas = false;

  if(!useBlas)
    return;

  auto& mesh = *obj.bucketId->staticMesh;
  if(auto b = mesh.blas(obj.iboOff, obj.iboLen)) {
    (void)b;
    notifyTlas(mat, toRtCategory(obj.type));
    }
  }

VisualObjects::Item VisualObjects::get(const StaticMesh& mesh, const Material& mat,
                                       size_t iboOff, size_t iboLen,
                                       bool staticDraw) {
  // return Item();
  // 64x64 meshlets
  assert(iboOff%PackedMesh::MaxInd==0);
  assert(iboLen%PackedMesh::MaxInd==0);

  if(mat.tex==nullptr) {
    Log::e("no texture?!");
    return VisualObjects::Item();
    }
  auto         type = (staticDraw ? DrawCommands::Static : DrawCommands::Movable);
  const size_t id     = implAlloc();

  Object& obj = objects[id];

  if(mesh.morph.anim!=nullptr) {
    type = DrawCommands::Morph;
    }

  obj.type      = type;
  obj.iboOff    = uint32_t(iboOff);
  obj.iboLen    = uint32_t(iboLen);
  obj.bucketId  = bucketsMem.alloc(mat, mesh);
  obj.cmdId     = drawCmd.commandId(mat, obj.type, obj.bucketId.toInt());
  obj.clusterId = clusterId(*obj.bucketId, iboOff/PackedMesh::MaxInd, iboLen/PackedMesh::MaxInd, obj.bucketId.toInt(), obj.cmdId);
  obj.alpha     = mat.alpha;
  obj.timeShift = -scene.tickCount;

  if(obj.isEmpty())
    return Item(); // null command

  obj.objInstance = instanceMem.alloc(sizeof(InstanceDesc));
  clustersMem[obj.clusterId].instanceId = obj.objInstance.offsetId<InstanceDesc>();

  if(type==DrawCommands::Morph) {
    obj.objMorphAnim = instanceMem.alloc(sizeof(MorphDesc)*Resources::MAX_MORPH_LAYERS);
    obj.animPtr      = obj.objMorphAnim.offsetId<MorphDesc>();
    const MorphData d = {};
    obj.objMorphAnim.set(&d, 0, sizeof(d));
    }

  updateInstance(id);
  updateRtAs(id);
  return Item(*this, id);
  }

VisualObjects::Item VisualObjects::get(const AnimMesh& mesh, const Material& mat,
                                       size_t iboOff, size_t iboLen,
                                       const InstanceStorage::Id& anim) {
  // return Item();
  // 64x64 meshlets
  assert(iboOff%PackedMesh::MaxInd==0);
  assert(iboLen%PackedMesh::MaxInd==0);

  if(mat.tex==nullptr) {
    Log::e("no texture?!");
    return VisualObjects::Item();
    }

  const size_t id = implAlloc();

  Object& obj = objects[id];

  obj.type      = DrawCommands::Type::Animated;
  obj.iboOff    = uint32_t(iboOff);
  obj.iboLen    = uint32_t(iboLen);
  obj.bucketId  = bucketsMem.alloc(mat, mesh);
  obj.cmdId     = drawCmd.commandId(mat, obj.type, obj.bucketId.toInt());
  obj.clusterId = clusterId(*obj.bucketId, iboOff/PackedMesh::MaxInd, iboLen/PackedMesh::MaxInd, obj.bucketId.toInt(), obj.cmdId);
  obj.alpha     = mat.alpha;
  obj.timeShift = -scene.tickCount;

  if(obj.isEmpty())
    return Item(); // null command
  obj.animPtr     = anim.offsetId<Matrix4x4>();
  obj.objInstance = instanceMem.alloc(sizeof(InstanceDesc));
  clustersMem[obj.clusterId].instanceId = obj.objInstance.offsetId<InstanceDesc>();

  updateInstance(id);
  updateRtAs(id);
  return Item(*this, id);
  }

VisualObjects::Item VisualObjects::get(const StaticMesh& mesh, const Material& mat,
                                       size_t iboOff, size_t iboLen, const PackedMesh::Cluster* cluster,
                                       DrawCommands::Type type) {
  // return Item();
  // 64x64 meshlets
  assert(iboOff%PackedMesh::MaxInd==0);
  assert(iboLen%PackedMesh::MaxInd==0);

  const size_t id = implAlloc();

  Object& obj = objects[id];

  obj.type      = DrawCommands::Type::Landscape;
  obj.iboOff    = uint32_t(iboOff);
  obj.iboLen    = uint32_t(iboLen);
  obj.bucketId  = bucketsMem.alloc(mat, mesh);
  obj.cmdId     = drawCmd.commandId(mat, type, obj.bucketId.toInt());
  obj.clusterId = clusterId(cluster, iboOff/PackedMesh::MaxInd, iboLen/PackedMesh::MaxInd, obj.bucketId.toInt(), obj.cmdId);
  obj.alpha     = mat.alpha;
  obj.timeShift = -scene.tickCount;

  if(obj.isEmpty())
    return Item(); // null command
  updateRtAs(id);
  return Item(*this, id);
  }

size_t VisualObjects::implAlloc() {
  if(!objectsFree.empty()) {
    auto id = *objectsFree.begin();
    objectsFree.erase(objectsFree.begin());
    return id;
    }

  objects.resize(objects.size()+1);
  return objects.size()-1;
  }

void VisualObjects::free(size_t id) {
  Object& obj = objects[id];

  const uint32_t meshletCount = (obj.iboLen/PackedMesh::MaxInd);
  const uint32_t numCluster = (obj.type==DrawCommands::Landscape ? meshletCount : 1);

  drawCmd.addClusters(obj.cmdId, -meshletCount);
  clustersMem.free(obj.clusterId, numCluster);

  if(obj.wind==zenkit::AnimationType::NONE)
    objectsWind.erase(id);
  if(obj.type==DrawCommands::Morph)
    objectsMorph.erase(id);

  obj = Object();
  while(objects.size()>0) {
    if(!objects.back().isEmpty())
      break;
    objects.pop_back();
    objectsFree.erase(objects.size());
    }
  if(id<objects.size())
    objectsFree.insert(id);
  }

InstanceStorage::Id VisualObjects::alloc(size_t size) {
  return instanceMem.alloc(size);
  }

bool VisualObjects::realloc(InstanceStorage::Id& id, size_t size) {
  return instanceMem.realloc(id, size);
  }

void VisualObjects::resetRendering() {
  return drawCmd.resetRendering();
  }

const Tempest::StorageBuffer& VisualObjects::instanceSsbo() const {
  return instanceMem.ssbo();
  }

uint32_t VisualObjects::clusterId(const PackedMesh::Cluster* cx, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId) {
  if(commandId==uint16_t(-1))
    return uint32_t(-1);

  const auto ret = clustersMem.alloc(cx, firstMeshlet, meshletCount, bucketId, commandId);
  drawCmd.addClusters(commandId, uint32_t(meshletCount));
  return uint32_t(ret);
  }

uint32_t VisualObjects::clusterId(const DrawBuckets::Bucket& bucket, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId) {
  if(commandId==uint16_t(-1))
    return uint32_t(-1);

  const auto ret = clustersMem.alloc(bucket, firstMeshlet, meshletCount, bucketId, commandId);
  drawCmd.addClusters(commandId, uint32_t(meshletCount));
  return uint32_t(ret);
  }

void VisualObjects::startMMAnim(size_t i, std::string_view animName, float intensity, uint64_t timeUntil) {
  auto& obj        = objects[i];
  auto  staticMesh = obj.bucketId->staticMesh;
  if(staticMesh==nullptr || staticMesh->morph.anim==nullptr)
    return;

  auto&  anim = *staticMesh->morph.anim;
  size_t id   = size_t(-1);
  for(size_t i=0; i<anim.size(); ++i)
    if(anim[i].name==animName) {
      id = i;
      break;
      }

  if(id==size_t(-1))
    return;

  objectsMorph.insert(i);

  auto& m = anim[id];
  if(timeUntil==uint64_t(-1) && m.duration>0)
    timeUntil = scene.tickCount + m.duration;

  // extend time of anim
  for(auto& i:obj.morphAnim) {
    if(i.id!=id || i.timeUntil<scene.tickCount)
      continue;
    i.timeUntil = timeUntil;
    i.intensity = intensity;
    return;
    }

  // find same layer
  for(auto& i:obj.morphAnim) {
    if(i.timeUntil<scene.tickCount)
      continue;
    if(anim[i.id].layer!=m.layer)
      continue;
    i.id        = id;
    i.timeUntil = timeUntil;
    i.intensity = intensity;
    return;
    }

  size_t nId = 0;
  for(size_t i=0; i<Resources::MAX_MORPH_LAYERS; ++i) {
    if(obj.morphAnim[nId].timeStart<=obj.morphAnim[i].timeStart)
      continue;
    nId = i;
    }

  auto& ani = obj.morphAnim[nId];
  ani.id        = id;
  ani.timeStart = scene.tickCount;
  ani.timeUntil = timeUntil;
  ani.intensity = intensity;
  }

void VisualObjects::setAsGhost(size_t id, bool g) {
  auto& obj= objects[id];
  if(obj.isGhost==g)
    return;

  auto& bx  = *obj.bucketId;
  auto& cx  = drawCmd[obj.cmdId];
  auto  mat = bx.mat;

  const uint32_t meshletCount = (obj.iboLen/PackedMesh::MaxInd);
  drawCmd.addClusters(obj.cmdId, -meshletCount);

  mat.alpha   = g ? Material::Ghost : obj.alpha;
  obj.isGhost = g;

  if(bx.staticMesh!=nullptr)
    obj.bucketId = bucketsMem.alloc(mat, *bx.staticMesh); else
    obj.bucketId = bucketsMem.alloc(mat, *bx.animMesh);

  obj.cmdId = drawCmd.commandId(mat, cx.type, obj.bucketId.toInt());
  drawCmd.addClusters(obj.cmdId, +meshletCount);

  const uint32_t numCluster = (obj.type==DrawCommands::Landscape ? meshletCount : 1);
  for(size_t i=0; i<numCluster; ++i) {
    clustersMem[obj.clusterId + i].commandId = obj.cmdId;
    clustersMem[obj.clusterId + i].bucketId  = obj.bucketId.toInt();
    clustersMem.markClusters(obj.clusterId + i);
    }
  }

void VisualObjects::prepareGlobals(Encoder<CommandBuffer>& enc, uint8_t fId) {
  instanceMem.commit(enc, fId);
  clustersMem.commit(enc, fId);
  drawCmd.commit(enc);
  if(bucketsMem.commit(enc, fId))
    drawCmd.updateBindlessArrays();
  }

void VisualObjects::preFrameUpdate() {
  preFrameUpdateWind();
  preFrameUpdateMorph();
  }

void VisualObjects::preFrameUpdateWind() {
  if(!scene.zWindEnabled)
    return;

  const uint64_t period = scene.windPeriod;
  float ax = float(scene.tickCount%period)/float(period);
  ax = ax*2.f-1.f;

  for(auto id:objectsWind) {
    auto& i = objects[id];
    float m = 0;

    switch(i.wind) {
      case zenkit::AnimationType::WIND:
        // tree. note: mods tent to bump Intensity to insane values
        if(i.windIntensity>0.f)
          m = 0.03f; else
          m = 0;
        break;
      case zenkit::AnimationType::WIND_ALT:
        // grass
        if(i.windIntensity>0.f && i.windIntensity<=1.0)
          m = i.windIntensity * 0.1f; else
          m = 0;
        break;
      case zenkit::AnimationType::NONE:
      default:
        // error
        m = 0.f;
        break;
      }

    auto  pos   = i.pos;
    float shift = i.pos[3][0]*scene.windDir.x + i.pos[3][2]*scene.windDir.y;
    const float a = m * std::cos(float(ax*M_PI) + shift*0.0001f);

    pos[1][0] += scene.windDir.x*a;
    pos[1][2] += scene.windDir.y*a;

    updateInstance(id, &pos);
    }
  }

void VisualObjects::preFrameUpdateMorph() {
  for(auto it=objectsMorph.begin(); it!=objectsMorph.end(); ) {
    auto& obj = objects[*it];

    bool reschedule = false;
    MorphData data = {};
    for(size_t i=0; i<Resources::MAX_MORPH_LAYERS; ++i) {
      auto&    ani  = obj.morphAnim[i];
      auto&    bk   = *obj.bucketId;
      auto&    anim = (*bk.staticMesh->morph.anim)[ani.id];
      uint64_t time = (scene.tickCount-ani.timeStart);

      float    alpha     = float(time%anim.tickPerFrame)/float(anim.tickPerFrame);
      float    intensity = ani.intensity;

      if(scene.tickCount>ani.timeUntil) {
        data.morph[i].intensity = 0;
        continue;
        }

      if(anim.numFrames>1)
        reschedule = true;

      const uint32_t samplesPerFrame = uint32_t(anim.samplesPerFrame);
      data.morph[i].indexOffset = uint32_t(anim.index);
      data.morph[i].sample0     = uint32_t((time/anim.tickPerFrame+0)%anim.numFrames)*samplesPerFrame;
      data.morph[i].sample1     = uint32_t((time/anim.tickPerFrame+1)%anim.numFrames)*samplesPerFrame;
      data.morph[i].alpha       = uint16_t(alpha*uint16_t(-1));
      data.morph[i].intensity   = uint16_t(intensity*uint16_t(-1));
      }

    obj.objMorphAnim.set(&data, 0, sizeof(data));
    if(!reschedule)
      it = objectsMorph.erase(it); else
      it++;
    }
  }

void VisualObjects::visibilityPass(Tempest::Encoder<Tempest::CommandBuffer> &cmd, int pass) {
  drawCmd.visibilityPass(cmd, pass);
  }

void VisualObjects::visibilityVsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  drawCmd.visibilityVsm(cmd);
  }

void VisualObjects::drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  // return;
  drawCmd.drawCommon(cmd, SceneGlobals::V_Main, Material::Multiply);
  drawCmd.drawCommon(cmd, SceneGlobals::V_Main, Material::Multiply2);
  drawCmd.drawCommon(cmd, SceneGlobals::V_Main, Material::Ghost);
  drawCmd.drawCommon(cmd, SceneGlobals::V_Main, Material::AdditiveLight);
  drawCmd.drawCommon(cmd, SceneGlobals::V_Main, Material::Transparent);
  }

void VisualObjects::drawWater(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  // return;
  drawCmd.drawCommon(cmd, SceneGlobals::V_Main, Material::Water);
  }

void VisualObjects::drawGBuffer(Tempest::Encoder<CommandBuffer>& cmd) {
  // return;
  drawCmd.drawCommon(cmd, SceneGlobals::V_Main, Material::Solid);
  drawCmd.drawCommon(cmd, SceneGlobals::V_Main, Material::AlphaTest);
  }

void VisualObjects::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& cmd, int layer) {
  // return;
  auto view = SceneGlobals::VisCamera(SceneGlobals::V_Shadow0 + layer);
  drawCmd.drawCommon(cmd, view, Material::Solid);
  drawCmd.drawCommon(cmd, view, Material::AlphaTest);
  }

void VisualObjects::drawVsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  drawCmd.drawVsm(cmd);
  }

void VisualObjects::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  drawCmd.drawHiZ(cmd);
  }

void VisualObjects::notifyTlas(const Material& m, RtScene::Category cat) {
  scene.rtScene.notifyTlas(m,cat);
  }

void VisualObjects::postFrameupdate() {
  instanceMem.join();
  }

bool VisualObjects::updateRtScene(RtScene& out) {
  if(!out.isUpdateRequired())
    return false;
  for(auto& obj:objects) {
    if(obj.isEmpty())
      continue;
    auto& bucket = *obj.bucketId;
    auto* mesh   = bucket.staticMesh;
    auto& mat    = bucket.mat;
    if(mesh==nullptr)
      continue;
    if(auto blas = mesh->blas(obj.iboOff, obj.iboLen)) {
      out.addInstance(obj.pos, *blas, mat, *mesh, obj.iboOff, obj.iboLen, toRtCategory(obj.type));
      }
    }
  out.buildTlas();
  return true;
  }

void VisualObjects::dbgClusters(Tempest::Painter& p, Vec2 wsz) {
  auto cam = Gothic::inst().camera();
  if(cam==nullptr)
    return;
  /*
  for(auto& c:clusters) {
    dbgDraw(p, wsz, *cam, c);
    }
  */

  /*
  if(auto pl = Gothic::inst().player()) {
    Cluster cx;
    cx.pos = pl->position();
    cx.r   = 100;
    dbgDraw(p, wsz, *cam, cx);
    }
  */
  }

void VisualObjects::dbgDraw(Tempest::Painter& p, Vec2 wsz, const Camera& cam, const DrawClusters::Cluster& cx) {
  auto  c       = cx.pos;
  auto  project = cam.projective();

  cam.view().project(c);
  //const vec3  c     = (scene.view * vec4(sphere.xyz, 1)).xyz;
  const float R     = cx.r;
  const float znear = cam.zNear();
  if(c.z - R < znear)
    ;//return;

  // depthMin = znear / (c.z + r);
  // float z = c.z + R;
  // depthMin  = scene.project[3][2]/z + scene.project[2][2];

  float P00 = project[0][0];
  float P11 = project[1][1];

  Vec3  cr   = c * R;
  float czr2 = c.z * c.z - R * R;

  float vx   = std::sqrt(c.x * c.x + czr2);
  float minx = (vx * c.x - cr.z) / (vx * c.z + cr.x);
  float maxx = (vx * c.x + cr.z) / (vx * c.z - cr.x);

  float vy   = std::sqrt(c.y * c.y + czr2);
  float miny = (vy * c.y - cr.z) / (vy * c.z + cr.y);
  float maxy = (vy * c.y + cr.z) / (vy * c.z - cr.y);

  Vec4 aabb;
  aabb = Vec4(minx * P00, miny * P11, maxx * P00, maxy * P11);
  aabb = aabb*0.5 + Vec4(0.5);

  aabb.x = aabb.x * wsz.x;
  aabb.z = aabb.z * wsz.x;

  aabb.y = aabb.y * wsz.y;
  aabb.w = aabb.w * wsz.y;

  if(aabb.x>=aabb.z)
    Log::d("");
  if(aabb.y>=aabb.w)
    Log::d("");

  p.setBrush(Color(0,0,1,0.1f));
  p.drawRect(int(aabb.x), int(aabb.y), int(aabb.z-aabb.x), int(aabb.w-aabb.y));
  }

void VisualObjects::dbgDrawBBox(Tempest::Painter& p, Tempest::Vec2 wsz, const Camera& cam, const DrawClusters::Cluster& c) {
  /*
  auto& b       = buckets[c.bucketId];
  auto  project = cam.viewProj();

  Vec4  aabb     = Vec4(1, 1, -1, -1);
  float depthMin = 1;
  for(uint32_t i=0; i<8; ++i) {
    float x = b[i&0x1 ? 1 : 0].x;
    float y = b[i&0x2 ? 1 : 0].y;
    float z = b[i&0x4 ? 1 : 0].z;

    const Vec3 pos = Vec3(x, y, z);
    Vec4 trPos = Vec4(pos,1.0);
    trPos = Vec4(obj.mat*trPos, 1.0);
    trPos = scene.viewProject*trPos;
    if(trPos.w<znear || false) {
      depthMin = 0;
      aabb     = vec4(0,0,1,1);
      return true;
      }

    vec3 bp = trPos.xyz/trPos.w;

    aabb.xy  = min(aabb.xy,  bp.xy);
    aabb.zw  = max(aabb.zw,  bp.xy);
    depthMin = min(depthMin, bp.z);
    }
  aabb = aabb*0.5 + Vec4(0.5);

  aabb.x = aabb.x * wsz.x;
  aabb.z = aabb.z * wsz.x;

  aabb.y = aabb.y * wsz.y;
  aabb.w = aabb.w * wsz.y;

  if(aabb.x>=aabb.z)
    Log::d("");
  if(aabb.y>=aabb.w)
    Log::d("");

  p.setBrush(Color(0,0,1,0.1f));
  p.drawRect(int(aabb.x), int(aabb.y), int(aabb.z-aabb.x), int(aabb.w-aabb.y));
  */
  }

