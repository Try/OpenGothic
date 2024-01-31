#include "drawcommands.h"

#include "graphics/mesh/submesh/animmesh.h"
#include "graphics/mesh/submesh/staticmesh.h"
#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/visualobjects.h"
#include "shaders.h"

using namespace Tempest;


bool DrawCommands::DrawCmd::isForwardShading() const {
  return Material::isForwardShading(alpha);
  }

bool DrawCommands::DrawCmd::isShadowmapRequired() const {
  return Material::isShadowmapRequired(alpha);
  }

bool DrawCommands::DrawCmd::isSceneInfoRequired() const {
  return Material::isSceneInfoRequired(alpha);
  }

bool DrawCommands::DrawCmd::isTextureInShadowPass() const {
  return Material::isTextureInShadowPass(alpha);
  }


DrawCommands::DrawCommands(VisualObjects& owner, DrawClusters& clusters, const SceneGlobals& scene) : owner(owner), clusters(clusters), scene(scene) {
  tasks.clear();
  for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
    TaskCmd cmd;
    cmd.viewport = SceneGlobals::VisCamera(v);
    tasks.emplace_back(std::move(cmd));
    }
  }

DrawCommands::~DrawCommands() {
  }

uint16_t DrawCommands::commandId(const Material& m, Type type, uint32_t bucketId) {
  auto pMain   = Shaders::inst().materialPipeline(m, type, Shaders::T_Main);
  auto pShadow = Shaders::inst().materialPipeline(m, type, Shaders::T_Shadow);
  auto pHiZ    = Shaders::inst().materialPipeline(m, type, Shaders::T_Depth);
  if(pMain==nullptr && pShadow==nullptr && pHiZ==nullptr)
    return uint16_t(-1);

  const bool bindless = true;

  for(size_t i=0; i<cmd.size(); ++i) {
    if(cmd[i].pMain!=pMain || cmd[i].pShadow!=pShadow || cmd[i].pHiZ!=pHiZ)
      continue;
    if(!bindless && cmd[i].bucketId != bucketId)
      continue;
    return uint16_t(i);
    }

  auto ret = uint16_t(cmd.size());

  auto& device = Resources::device();
  DrawCmd cx;
  cx.pMain       = pMain;
  cx.pShadow     = pShadow;
  cx.pHiZ        = pHiZ;
  cx.bucketId    = bindless ? 0xFFFFFFFF : bucketId;
  cx.type        = type;
  cx.alpha       = m.alpha;
  if(cx.pMain!=nullptr) {
    cx.desc[SceneGlobals::V_Main] = device.descriptors(*cx.pMain);
    }
  if(cx.pShadow!=nullptr) {
    cx.desc[SceneGlobals::V_Shadow0] = device.descriptors(*cx.pShadow);
    cx.desc[SceneGlobals::V_Shadow1] = device.descriptors(*cx.pShadow);
    }
  if(cx.pHiZ!=nullptr) {
    cx.desc[SceneGlobals::V_HiZ] = device.descriptors(*cx.pHiZ);
    }
  cmd.push_back(std::move(cx));
  cmdDurtyBit = true;
  return ret;
  }

void DrawCommands::addClusters(uint16_t cmdId, uint32_t meshletCount) {
  cmdDurtyBit = true;
  cmd[cmdId].maxPayload += meshletCount;
  }

bool DrawCommands::commit() {
  if(!cmdDurtyBit)
    return false;
  cmdDurtyBit = false;

  totalPayload = 0;
  for(auto& i:cmd) {
    i.firstPayload = uint32_t(totalPayload);
    totalPayload  += i.maxPayload;
    }

  std::vector<IndirectCmd> cx(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i) {
    cx[i].vertexCount = PackedMesh::MaxInd;
    cx[i].writeOffset = cmd[i].firstPayload;
    }

  ord.resize(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i)
    ord[i] = &cmd[i];
  std::sort(ord.begin(), ord.end(), cmpDraw);

  auto& device = Resources::device();
  for(auto& v:views) {
    Resources::recycle(std::move(v.visClusters));
    Resources::recycle(std::move(v.indirectCmd));
    Resources::recycle(std::move(v.descInit));

    v.visClusters = device.ssbo(nullptr, totalPayload*sizeof(uint32_t)*4);
    v.indirectCmd = device.ssbo(cx.data(), sizeof(IndirectCmd)*cx.size());

    v.descInit = device.descriptors(Shaders::inst().clusterInit);
    v.descInit.set(T_Indirect, v.indirectCmd);
    }

  return true;
  }

bool DrawCommands::cmpDraw(const DrawCmd* l, const DrawCmd* r) {
  return l->alpha < r->alpha;
  }

void DrawCommands::invalidateUbo() {
  if(owner.instanceSsbo().isEmpty())
    return;

  auto& device = Resources::device();
  device.waitIdle(); // TODO

  std::vector<const Tempest::Texture2d*>     tex;
  std::vector<const Tempest::StorageBuffer*> vbo, ibo;
  std::vector<const Tempest::StorageBuffer*> morphId, morph;
  for(auto& i:owner.buckets()) {
    tex.push_back(i.mat.tex);
    if(i.staticMesh!=nullptr) {
      ibo    .push_back(&i.staticMesh->ibo8);
      vbo    .push_back(&i.staticMesh->vbo);
      morphId.push_back(i.staticMesh->morph.index);
      morph  .push_back(i.staticMesh->morph.samples);
      } else {
      ibo    .push_back(&i.animMesh->ibo8);
      vbo    .push_back(&i.animMesh->vbo);
      morphId.push_back(nullptr);
      morph  .push_back(nullptr);
      }
    }

  for(auto& i:tasks) {
    Resources::recycle(std::move(i.desc));
    if(i.viewport==SceneGlobals::V_Main)
      i.desc = device.descriptors(Shaders::inst().clusterTaskHiZ);
    else if(i.viewport==SceneGlobals::V_HiZ)
      i.desc = device.descriptors(Shaders::inst().clusterTaskHiZCr);
    else
      i.desc = device.descriptors(Shaders::inst().clusterTask);
    i.desc.set(T_Clusters, clusters.ssbo());
    i.desc.set(T_Indirect, views[i.viewport].indirectCmd);
    i.desc.set(T_Payload,  views[i.viewport].visClusters);

    i.desc.set(T_Scene,    scene.uboGlobal[i.viewport]);
    i.desc.set(T_Instance, owner.instanceSsbo());
    i.desc.set(T_Bucket,   owner.bucketsSsbo());
    i.desc.set(T_HiZ,      *scene.hiZ);
    }

  for(auto& i:cmd) {
    for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
      if(i.desc[v].isEmpty())
        continue;
      auto& mem = (i.type==Type::Landscape) ? clusters.ssbo() : owner.instanceSsbo();

      i.desc[v].set(L_Scene,    scene.uboGlobal[v]);
      i.desc[v].set(L_Instance, mem);
      i.desc[v].set(L_Ibo,      ibo);
      i.desc[v].set(L_Vbo,      vbo);
      i.desc[v].set(L_Diffuse,  tex);
      i.desc[v].set(L_Bucket,   owner.bucketsSsbo());
      i.desc[v].set(L_Payload,  views[v].visClusters);
      i.desc[v].set(L_Sampler,  Sampler::anisotrophy());

      if(v==SceneGlobals::V_Main || i.isTextureInShadowPass()) {
        //i.desc[v].set(L_Diffuse, tex);
        }

      if(v==SceneGlobals::V_Main && i.isShadowmapRequired()) {
        i.desc[v].set(L_Shadow0, *scene.shadowMap[0],Resources::shadowSampler());
        i.desc[v].set(L_Shadow1, *scene.shadowMap[1],Resources::shadowSampler());
        }

      if(i.type==Morph) {
        i.desc[v].set(L_MorphId,  morphId);
        i.desc[v].set(L_Morph,    morph);
        }

      if(v==SceneGlobals::V_Main && i.isSceneInfoRequired()) {
        auto smp = Sampler::bilinear();
        smp.setClamping(ClampMode::MirroredRepeat);
        i.desc[v].set(L_SceneClr, *scene.sceneColor, smp);

        smp = Sampler::nearest();
        smp.setClamping(ClampMode::MirroredRepeat);
        i.desc[v].set(L_GDepth, *scene.sceneDepth, smp);
        }
      }
    }
  }

void DrawCommands::visibilityPass(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int pass) {
  static bool freeze = false;
  if(freeze)
    return;

  cmd.setFramebuffer({});
  if(pass==0) {
    for(auto& v:views) {
      if(this->cmd.empty())
        continue;
      cmd.setUniforms(Shaders::inst().clusterInit, v.descInit);
      cmd.dispatchThreads(this->cmd.size());
      }
    }

  for(auto& i:tasks) {
    if(i.viewport==SceneGlobals::V_HiZ && pass!=0)
      continue;
    if(i.viewport!=SceneGlobals::V_HiZ && pass==0)
      continue;
    struct Push { uint32_t firstMeshlet; uint32_t meshletCount; float znear; } push = {};
    push.firstMeshlet = 0;
    push.meshletCount = uint32_t(clusters.size());
    push.znear        = scene.znear;

    auto* pso = &Shaders::inst().clusterTask;
    if(i.viewport==SceneGlobals::V_Main)
      pso = &Shaders::inst().clusterTaskHiZ;
    else if(i.viewport==SceneGlobals::V_HiZ)
      pso = &Shaders::inst().clusterTaskHiZCr;
    cmd.setUniforms(*pso, i.desc, &push, sizeof(push));
    cmd.dispatchThreads(push.meshletCount);
    }
  }

void DrawCommands::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  // return;
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  auto  viewId = SceneGlobals::V_HiZ;
  auto& view   = views[viewId];
  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.desc[viewId].isEmpty())
      continue;
    if(cx.alpha!=Material::Solid && cx.alpha!=Material::AlphaTest)
      continue;
    if(cx.type!=Landscape && cx.type!=Static)
      continue;
    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstPayload;
    push.meshletCount = cx.maxPayload;

    cmd.setUniforms(*cx.pHiZ, cx.desc[viewId], &push, sizeof(push));
    cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

void DrawCommands::drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, SceneGlobals::VisCamera viewId, Material::AlphaFunc func) {
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  auto& view = views[viewId];
  // auto  b    = std::lower_bound(ord.begin(), ord.end(), cmpDraw);
  // auto  e    = std::upper_bound(ord.begin(), ord.end(), cmpDraw);
  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.desc[viewId].isEmpty())
      continue;
    if(cx.alpha!=func)
      continue;

    const RenderPipeline* pso = nullptr;
    switch(viewId) {
      case SceneGlobals::V_Shadow0:
      case SceneGlobals::V_Shadow1:
        pso = cx.pShadow;
        break;
      case SceneGlobals::V_Main:
        pso = cx.pMain;
        break;
      case SceneGlobals::V_HiZ:
      case SceneGlobals::V_Count:
        break;
      }
    if(pso==nullptr)
      continue;

    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstPayload;
    push.meshletCount = cx.maxPayload;

    cmd.setUniforms(*pso, cx.desc[viewId], &push, sizeof(push));
    cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }
