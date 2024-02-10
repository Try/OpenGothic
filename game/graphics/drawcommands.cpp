#include "drawcommands.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/animmesh.h"
#include "graphics/mesh/submesh/staticmesh.h"
#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/visualobjects.h"
#include "shaders.h"

#include "gothic.h"

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

bool DrawCommands::DrawCmd::isBindless() const {
  return bucketId==uint32_t(-1);
  }


DrawCommands::DrawCommands(VisualObjects& owner, DrawBuckets& buckets, DrawClusters& clusters, const SceneGlobals& scene)
    : owner(owner), buckets(buckets), clusters(clusters), scene(scene) {
  tasks.clear();
  for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
    TaskCmd cmd;
    cmd.viewport = SceneGlobals::VisCamera(v);
    tasks.emplace_back(std::move(cmd));
    }
  }

DrawCommands::~DrawCommands() {
  }

bool DrawCommands::cmpDraw(const DrawCmd* l, const DrawCmd* r) {
  return l->alpha < r->alpha;
  }

uint16_t DrawCommands::commandId(const Material& m, Type type, uint32_t bucketId) {
  const bool bindlessSys = Gothic::inst().options().doBindless;
  const bool bindless    = bindlessSys && !m.hasFrameAnimation();

  auto pMain    = Shaders::inst().materialPipeline(m, type, Shaders::T_Main,   bindless);
  auto pShadow  = Shaders::inst().materialPipeline(m, type, Shaders::T_Shadow, bindless);
  auto pHiZ     = Shaders::inst().materialPipeline(m, type, Shaders::T_Depth,  bindless);
  if(pMain==nullptr && pShadow==nullptr && pHiZ==nullptr)
    return uint16_t(-1);

  for(size_t i=0; i<cmd.size(); ++i) {
    if(cmd[i].pMain!=pMain || cmd[i].pShadow!=pShadow || cmd[i].pHiZ!=pHiZ)
      continue;
    if(!bindless && cmd[i].bucketId!=bucketId)
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

  for(uint8_t fId=0; fId<Resources::MaxFramesInFlight; ++fId) {
    auto& desc = cx.isBindless() ? cx.desc : cx.descFr[fId];
    if(cx.pMain!=nullptr) {
      desc[SceneGlobals::V_Main] = device.descriptors(*cx.pMain);
      }
    if(cx.pShadow!=nullptr) {
      desc[SceneGlobals::V_Shadow0] = device.descriptors(*cx.pShadow);
      desc[SceneGlobals::V_Shadow1] = device.descriptors(*cx.pShadow);
      }
    if(cx.pHiZ!=nullptr) {
      desc[SceneGlobals::V_HiZ] = device.descriptors(*cx.pHiZ);
      }

    if(cx.isBindless())
      break;
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

  size_t totalPayload = 0;
  bool   layChg       = false;
  for(auto& i:cmd) {
    layChg |= (i.firstPayload != uint32_t(totalPayload));
    i.firstPayload = uint32_t(totalPayload);
    totalPayload  += i.maxPayload;
    }

  totalPayload = (totalPayload + 255) & ~size_t(255);
  const size_t visClustersSz = totalPayload*sizeof(uint32_t)*4;

  auto& v      = views[0];
  bool  cmdChg = true; //v.indirectCmd.byteSize()!=sizeof(IndirectCmd)*cmd.size();
  bool  visChg = v.visClusters.byteSize()!=visClustersSz;
  if(!layChg && !visChg) {
    //return false;
    }

  const bool gmesh = Gothic::options().doMeshShading;
  std::vector<IndirectCmd> cx(cmd.size()+1);
  for(size_t i=0; i<cmd.size(); ++i) {
    auto mesh = gmesh && !Material::isTesselated(cmd[i].alpha);

    cx[i].vertexCount   = PackedMesh::MaxInd;
    cx[i].writeOffset   = cmd[i].firstPayload;
    cx[i].firstVertex   = mesh ? 1 : 0;
    cx[i].firstInstance = mesh ? 1 : 0;
    }
  cx.back().writeOffset = uint32_t(totalPayload);

  ord.resize(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i)
    ord[i] = &cmd[i];
  std::sort(ord.begin(), ord.end(), cmpDraw);

  auto& device = Resources::device();
  for(auto& v:views) {
    if(visChg) {
      Resources::recycle(std::move(v.visClusters));
      v.visClusters = device.ssbo(nullptr, visClustersSz);
      }
    if(cmdChg) {
      Resources::recycle(std::move(v.indirectCmd));
      v.indirectCmd = device.ssbo(cx.data(), sizeof(IndirectCmd)*cx.size());
      }

    Resources::recycle(std::move(v.descInit));
    v.descInit = device.descriptors(Shaders::inst().clusterInit);
    //v.descInit.set(T_Bucket,   buckets.ssbo());
    v.descInit.set(T_Indirect, v.indirectCmd);
    }

  if(!visChg) {
    updateTasksUniforms();
    return false;
    }

  return true;
  }

void DrawCommands::updateTasksUniforms() {
  auto& device = Resources::device();
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
    i.desc.set(T_Bucket,   buckets.ssbo());
    i.desc.set(T_HiZ,      *scene.hiZ);
    }
  }

void DrawCommands::updateCommandUniforms() {
  auto& device = Resources::device();
  device.waitIdle(); // TODO

  std::vector<const Tempest::Texture2d*>     tex;
  std::vector<const Tempest::StorageBuffer*> vbo, ibo;
  std::vector<const Tempest::StorageBuffer*> morphId, morph;
  for(auto& i:buckets.buckets()) {
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

  for(auto& cx:cmd) {
    for(uint8_t fId=0; fId<Resources::MaxFramesInFlight; ++fId) {
      for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
        auto& desc = cx.isBindless() ? cx.desc : cx.descFr[fId];
        if(desc[v].isEmpty())
          continue;

        auto  bId = cx.bucketId;
        auto& mem = (cx.type==Type::Landscape) ? clusters.ssbo() : owner.instanceSsbo();
        desc[v].set(L_Scene,    scene.uboGlobal[v]);
        desc[v].set(L_Payload,  views[v].visClusters);
        desc[v].set(L_Instance, mem);
        desc[v].set(L_Bucket,   buckets.ssbo());

        if(cx.isBindless()) {
          desc[v].set(L_Ibo,    ibo);
          desc[v].set(L_Vbo,    vbo);
          } else {
          desc[v].set(L_Ibo,    *ibo[bId]);
          desc[v].set(L_Vbo,    *vbo[bId]);
          }

        if(v==SceneGlobals::V_Main || cx.isTextureInShadowPass()) {
          if(cx.isBindless())
            desc[v].set(L_Diffuse, tex); else
            desc[v].set(L_Diffuse, *tex[bId]);
          desc[v].set(L_Sampler, Sampler::anisotrophy());
          }

        if(v==SceneGlobals::V_Main && cx.isShadowmapRequired()) {
          desc[v].set(L_Shadow0, *scene.shadowMap[0],Resources::shadowSampler());
          desc[v].set(L_Shadow1, *scene.shadowMap[1],Resources::shadowSampler());
          }

        if(cx.type==Morph && cx.isBindless()) {
          desc[v].set(L_MorphId,  morphId);
          desc[v].set(L_Morph,    morph);
          }
        else if(cx.type==Morph) {
          desc[v].set(L_MorphId,  *morphId[bId]);
          desc[v].set(L_Morph,    *morph[bId]);
          }

        if(v==SceneGlobals::V_Main && cx.isSceneInfoRequired()) {
          auto smp = Sampler::bilinear();
          smp.setClamping(ClampMode::MirroredRepeat);
          desc[v].set(L_SceneClr, *scene.sceneColor, smp);

          smp = Sampler::nearest();
          smp.setClamping(ClampMode::MirroredRepeat);
          desc[v].set(L_GDepth, *scene.sceneDepth, smp);
          }
        }

      if(cx.isBindless())
        break;
      }
    }
  }

void DrawCommands::prepareUniforms() {
  // TODO
  updateUniforms();
  }

void DrawCommands::preFrameUpdate(uint8_t fId) {
  for(auto& cx:cmd) {
    if(cx.isBindless())
      continue;

    auto& desc = cx.descFr[fId];
    for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
      if(desc[v].isEmpty())
        continue;

      auto& bucket = buckets[cx.bucketId];
      auto& mat    = bucket.mat;
      if(mat.hasFrameAnimation()) {
        uint64_t timeShift = 0;
        auto  frame  = size_t((timeShift+scene.tickCount)/mat.texAniFPSInv);
        auto  t     = mat.frames[frame%mat.frames.size()];
        if(v==SceneGlobals::V_Main || mat.isTextureInShadowPass()) {
          desc[v].set(L_Diffuse, *t);
          }
        }
      }
    }
  }

void DrawCommands::updateUniforms() {
  if(owner.instanceSsbo().isEmpty())
    return;

  updateTasksUniforms();
  updateCommandUniforms();
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
      const uint32_t isMeshShader = (Gothic::options().doMeshShading ? 1 : 0);
      cmd.setUniforms(Shaders::inst().clusterInit, v.descInit, &isMeshShader, sizeof(isMeshShader));
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

  const bool mesh   = Gothic::options().doMeshShading;
  auto       viewId = SceneGlobals::V_HiZ;
  auto&      view   = views[viewId];

  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.alpha!=Material::Solid && cx.alpha!=Material::AlphaTest)
      continue;
    if(cx.type!=Landscape && cx.type!=Static)
      continue;

    auto& desc = cx.isBindless() ? cx.desc : cx.descFr[fId];
    if(desc[viewId].isEmpty())
      continue;

    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstPayload;
    push.meshletCount = cx.maxPayload;

    cmd.setUniforms(*cx.pHiZ, desc[viewId], &push, sizeof(push));
    if(mesh)
      cmd.dispatchMeshIndirect(view.indirectCmd, sizeof(IndirectCmd)*id + sizeof(uint32_t)); else
      cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

void DrawCommands::drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, SceneGlobals::VisCamera viewId, Material::AlphaFunc func) {
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  const bool mesh = Gothic::options().doMeshShading;
  auto&      view = views[viewId];
  // auto  b    = std::lower_bound(ord.begin(), ord.end(), cmpDraw);
  // auto  e    = std::upper_bound(ord.begin(), ord.end(), cmpDraw);
  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.alpha!=func)
      continue;

    auto& desc = cx.isBindless() ? cx.desc : cx.descFr[fId];
    if(desc[viewId].isEmpty())
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

    cmd.setUniforms(*pso, desc[viewId], &push, sizeof(push));
    if(mesh && !Material::isTesselated(cx.alpha))
      cmd.dispatchMeshIndirect(view.indirectCmd, sizeof(IndirectCmd)*id + sizeof(uint32_t)); else
      cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }
