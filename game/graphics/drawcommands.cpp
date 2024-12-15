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

bool DrawCommands::DrawCmd::isMeshShader() const {
  auto& opt = Gothic::inst().options();
  if(!opt.doMeshShading)
    return false;
  if(Material::isTesselated(alpha) && Resources::device().properties().tesselationShader)
    return false;
  return true;
  }


bool DrawCommands::View::isEnabled() const {
  const bool virtualShadowSys = Gothic::inst().options().doVirtualShadow;
  if(viewport==SceneGlobals::V_Vsm && !virtualShadowSys)
    return false;
  return true;
  }


DrawCommands::DrawCommands(VisualObjects& owner, DrawBuckets& buckets, DrawClusters& clusters, const SceneGlobals& scene)
    : owner(owner), buckets(buckets), clusters(clusters), scene(scene) {
  const bool virtualShadowSys = Gothic::inst().options().doVirtualShadow;
  for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
    views[v].viewport = SceneGlobals::VisCamera(v);
    }
  tasks.clear();
  for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
    if(v==SceneGlobals::V_Vsm && !virtualShadowSys)
      continue;
    TaskCmd cmd;
    cmd.viewport = SceneGlobals::VisCamera(v);
    tasks.emplace_back(std::move(cmd));
    }

  if(virtualShadowSys) {
    Tempest::DispatchIndirectCommand cmd = {2000,1,1};
    vsmIndirectCmd = Resources::device().ssbo(&cmd, sizeof(cmd));
    }
  // vsmSwrImage = Resources::device().image2d(TextureFormat::R16,  4096, 4096);
  // vsmSwrImage = Resources::device().image2d(TextureFormat::R32U, 4096, 4096);
  }

DrawCommands::~DrawCommands() {
  }

uint16_t DrawCommands::commandId(const Material& m, Type type, uint32_t bucketId) {
  const bool bindlessSys = Gothic::inst().options().doBindless;
  const bool bindless    = bindlessSys && !m.hasFrameAnimation();

  auto pMain    = Shaders::inst().materialPipeline(m, type, Shaders::T_Main,   bindless);
  auto pShadow  = Shaders::inst().materialPipeline(m, type, Shaders::T_Shadow, bindless);
  auto pVsm     = Shaders::inst().materialPipeline(m, type, Shaders::T_Vsm,    bindless);
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
  cx.pVsm        = pVsm;
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
    if(cx.pVsm!=nullptr) {
      desc[SceneGlobals::V_Vsm] = device.descriptors(*cx.pVsm);
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

bool DrawCommands::commit(Encoder<CommandBuffer>& enc, uint8_t fId) {
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

  totalPayload = (totalPayload + 0xFF) & ~size_t(0xFF);
  const size_t visClustersSz = totalPayload*sizeof(uint32_t)*4;

  auto& v      = views[0];
  bool  cmdChg = v.indirectCmd.byteSize()!=sizeof(IndirectCmd)*cmd.size();
  bool  visChg = v.visClusters.byteSize()!=visClustersSz;
  if(!layChg && !visChg) {
    //return false;
    }

  std::vector<IndirectCmd> cx(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i) {
    auto mesh = cmd[i].isMeshShader();

    cx[i].vertexCount   = PackedMesh::MaxInd;
    cx[i].writeOffset   = cmd[i].firstPayload;
    cx[i].firstVertex   = mesh ? 1 : 0;
    cx[i].firstInstance = mesh ? 1 : 0;
    }

  ord.resize(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i)
    ord[i] = &cmd[i];
  std::sort(ord.begin(), ord.end(), [](const DrawCmd* l, const DrawCmd* r){
    return l->alpha < r->alpha;
    });

  auto& device = Resources::device();
  for(auto& v:views) {
    if(!v.isEnabled())
      continue;

    if(visChg) {
      const size_t vsmMax = 1024*1024*4*4; // arbitrary: ~1k clusters per page
      const size_t size   = v.viewport==SceneGlobals::V_Vsm ? vsmMax : visClustersSz;
      Resources::recycle(std::move(v.visClusters));
      v.visClusters = device.ssbo(nullptr, size);
      }
    if(visChg && v.viewport==SceneGlobals::V_Vsm) {
      Resources::recycle(std::move(v.vsmClusters));
      v.vsmClusters = device.ssbo(nullptr, v.visClusters.byteSize());
      }
    if(cmdChg) {
      Resources::recycle(std::move(v.indirectCmd));
      v.indirectCmd = device.ssbo(cx.data(), sizeof(IndirectCmd)*cx.size());
      visChg |= (v.viewport==SceneGlobals::V_Vsm);
      }
    else if(layChg) {
      auto staging = device.ssbo(BufferHeap::Upload, cx.data(), sizeof(IndirectCmd)*cx.size());

      auto& sh   = Shaders::inst().copyBuf;
      auto  desc = device.descriptors(sh);
      desc.set(0, v.indirectCmd);
      desc.set(1, staging);
      enc.setUniforms(sh, desc);
      enc.dispatchThreads(staging.byteSize()/sizeof(uint32_t));

      Resources::recycle(std::move(staging));
      Resources::recycle(std::move(desc));
      }

    Resources::recycle(std::move(v.descInit));
    v.descInit = device.descriptors(Shaders::inst().clusterInit);
    v.descInit.set(T_Indirect, v.indirectCmd);
    }

  updateTasksUniforms();
  if(!visChg)
    return false;

  return true;
  }

void DrawCommands::updateTasksUniforms() {
  auto& device = Resources::device();
  for(auto& i:tasks) {
    Resources::recycle(std::move(i.desc));
    if(i.viewport==SceneGlobals::V_Main)
      i.desc = device.descriptors(Shaders::inst().visibilityPassHiZ);
    else if(i.viewport==SceneGlobals::V_HiZ)
      i.desc = device.descriptors(Shaders::inst().visibilityPassHiZCr);
    else if(i.viewport==SceneGlobals::V_Vsm)
      i.desc = device.descriptors(Shaders::inst().vsmVisibilityPass);
    else
      i.desc = device.descriptors(Shaders::inst().visibilityPassSh);
    i.desc.set(T_Clusters, clusters.ssbo());
    i.desc.set(T_Indirect, views[i.viewport].indirectCmd);
    i.desc.set(T_Payload,  views[i.viewport].visClusters);
    i.desc.set(T_Scene,    scene.uboGlobal[i.viewport]);
    i.desc.set(T_Instance, owner.instanceSsbo());
    i.desc.set(T_Bucket,   buckets.ssbo());

    if(i.viewport!=SceneGlobals::V_Vsm) {
      i.desc.set(T_HiZ,      *scene.hiZ);
      } else {
      i.desc.set(T_Payload,  views[i.viewport].vsmClusters); //unsorted clusters
      i.desc.set(T_Lights,   *scene.lights);
      i.desc.set(T_HiZ,      *scene.vsmPageHiZ);
      i.desc.set(T_VsmPages, *scene.vsmPageList);
      i.desc.set(9,          scene.vsmDbg);
      // i.desc.set(T_PkgOffsets, views[i.viewport].pkgOffsets);
      }
    }
  for(auto& v:views) {
    if(v.viewport!=SceneGlobals::V_Vsm)
      continue;
    Resources::recycle(std::move(v.descPackDraw0));
    v.descPackDraw0 = device.descriptors(Shaders::inst().vsmPackDraw0);
    v.descPackDraw0.set(1, v.vsmClusters);
    v.descPackDraw0.set(2, v.visClusters);
    v.descPackDraw0.set(3, v.indirectCmd);
    v.descPackDraw0.set(4, *scene.vsmPageList);
    v.descPackDraw0.set(5, vsmIndirectCmd);

    Resources::recycle(std::move(v.descPackDraw1));
    v.descPackDraw1 = device.descriptors(Shaders::inst().vsmPackDraw1);
    v.descPackDraw1.set(1, v.vsmClusters);
    v.descPackDraw1.set(2, v.visClusters);
    v.descPackDraw1.set(3, v.indirectCmd);
    v.descPackDraw1.set(4, *scene.vsmPageList);
    }

  updatsSwrUniforms();
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

        auto bId = cx.bucketId;
        desc[v].set(L_Scene,    scene.uboGlobal[v]);
        desc[v].set(L_Payload,  views[v].visClusters);
        desc[v].set(L_Instance, owner.instanceSsbo());
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
          auto smp = SceneGlobals::isShadowView(SceneGlobals::VisCamera(v)) ? Sampler::trillinear() : Sampler::anisotrophy();
          desc[v].set(L_Sampler, smp);
          }

        if(v==SceneGlobals::V_Main && cx.isShadowmapRequired()) {
          desc[v].set(L_Shadow0, *scene.shadowMap[0], Resources::shadowSampler());
          desc[v].set(L_Shadow1, *scene.shadowMap[1], Resources::shadowSampler());
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

        if(v==SceneGlobals::V_Vsm) {
          desc[v].set(L_CmdOffsets, views[v].indirectCmd);
          desc[v].set(L_VsmPages,   *scene.vsmPageList);
          desc[v].set(L_Lights,     *scene.lights);
          }
        }

      if(cx.isBindless())
        break;
      }
    }

  updatsSwrUniforms();
  }

void DrawCommands::updateLigtsUniforms() {
  //NOTE: causes dev-idle, need to rework
  updateCommandUniforms();
  updateTasksUniforms();
  }

void DrawCommands::updatsSwrUniforms() {
  if(Gothic::options().swRenderingPreset==0)
    return;

  auto& device = Resources::device();

  //FIXME: copy-paste
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

  const uint32_t preset = Gothic::options().swRenderingPreset;
  if(preset>0 && !Shaders::inst().swRendering.isEmpty() && scene.swMainImage!=nullptr) {
    Resources::recycle(std::move(swrDesc));
    swrDesc = device.descriptors(Shaders::inst().swRendering);
    swrDesc.set(0, *scene.swMainImage);
    swrDesc.set(1, scene.uboGlobal[SceneGlobals::V_Main]);
    swrDesc.set(2, *scene.gbufNormals);
    swrDesc.set(3, *scene.zbuffer);
    swrDesc.set(4, clusters.ssbo());
    swrDesc.set(5, ibo);
    swrDesc.set(6, vbo);
    swrDesc.set(7, tex);
    swrDesc.set(8, Sampler::bilinear());
    if(preset==3) {
      swrDesc.set(9,  *scene.lights);
      swrDesc.set(10, owner.instanceSsbo());
      }
    }

  if(false && Gothic::options().doVirtualShadow && scene.vsmPageList!=nullptr) {
    Resources::recycle(std::move(vsmDesc));
    vsmDesc = device.descriptors(Shaders::inst().vsmRendering);

    vsmDesc.set(0, *scene.vsmPageData);
    vsmDesc.set(1, scene.uboGlobal[SceneGlobals::V_Vsm]);
    vsmDesc.set(2, *scene.vsmPageList);
    vsmDesc.set(3, clusters.ssbo());
    vsmDesc.set(4, owner.instanceSsbo());
    vsmDesc.set(5, ibo);
    vsmDesc.set(6, vbo);
    vsmDesc.set(7, tex);
    vsmDesc.set(8, Sampler::bilinear());
    }
  }

void DrawCommands::updateUniforms(uint8_t fId) {
  for(auto& cx:cmd) {
    if(cx.isBindless())
      continue;

    auto& desc = cx.descFr[fId];
    for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
      if(desc[v].isEmpty())
        continue;

      auto& bucket = buckets[cx.bucketId];
      auto& mat    = bucket.mat;

      // desc[v].set(L_Instance, owner.instanceSsbo());
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

void DrawCommands::visibilityPass(Encoder<CommandBuffer>& cmd, uint8_t fId, int pass) {
  static bool freeze = false;
  if(freeze)
    return;

  cmd.setFramebuffer({});
  if(pass==0) {
    for(auto& v:views) {
      if(this->cmd.empty())
        continue;
      if(!v.isEnabled())
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
    if(i.viewport==SceneGlobals::V_Vsm)
      continue;
    struct Push { uint32_t firstMeshlet; uint32_t meshletCount; float znear; } push = {};
    push.firstMeshlet = 0;
    push.meshletCount = uint32_t(clusters.size());
    push.znear        = scene.znear;

    auto* pso = &Shaders::inst().visibilityPassSh;
    if(i.viewport==SceneGlobals::V_Main)
      pso = &Shaders::inst().visibilityPassHiZ;
    else if(i.viewport==SceneGlobals::V_HiZ)
      pso = &Shaders::inst().visibilityPassHiZCr;
    cmd.setUniforms(*pso, i.desc, &push, sizeof(push));
    cmd.dispatchThreads(push.meshletCount);
    }
  }

void DrawCommands::visibilityVsm(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  for(auto& i:tasks) {
    if(i.viewport!=SceneGlobals::V_Vsm)
      continue;

    struct Push { uint32_t meshletCount; } push = {};
    push.meshletCount = uint32_t(clusters.size());

    auto* pso = &Shaders::inst().vsmVisibilityPass;
    cmd.setUniforms(*pso, i.desc, &push, sizeof(push));
    cmd.dispatchThreads(push.meshletCount, size_t(scene.vsmPageTbl->d() + 1));
    }

  cmd.setUniforms(Shaders::inst().vsmPackDraw0, views[SceneGlobals::V_Vsm].descPackDraw0);
  cmd.dispatch(1);

  cmd.setUniforms(Shaders::inst().vsmPackDraw1, views[SceneGlobals::V_Vsm].descPackDraw1);
  cmd.dispatch(8096); // TODO: indirect
  // cmd.dispatchIndirect(vsmIndirectCmd, 0);
  }

void DrawCommands::drawVsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  // return;
  struct Push { uint32_t commandId; } push = {};

  auto       viewId = SceneGlobals::V_Vsm;
  auto&      view   = views[viewId];

  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.alpha!=Material::Solid && cx.alpha!=Material::AlphaTest)
      continue;

    auto& desc = cx.isBindless() ? cx.desc : cx.descFr[fId];
    if(desc[viewId].isEmpty())
      continue;

    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.commandId = uint32_t(id);

    cmd.setUniforms(*cx.pVsm, desc[viewId], &push, sizeof(push));
    if(cx.isMeshShader())
      cmd.dispatchMeshIndirect(view.indirectCmd, sizeof(IndirectCmd)*id + sizeof(uint32_t)); else
      cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }

  if(false) {
    cmd.setFramebuffer({});
    struct Push { uint32_t meshletCount; } push = {};
    push.meshletCount = uint32_t(clusters.size());
    cmd.setUniforms(Shaders::inst().vsmRendering, vsmDesc, &push, sizeof(push));
    // const auto sz = Shaders::inst().vsmRendering.workGroupSize();
    cmd.dispatch(1024u);
    }
  }

void DrawCommands::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  // return;
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

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
    if(cx.isMeshShader())
      cmd.dispatchMeshIndirect(view.indirectCmd, sizeof(IndirectCmd)*id + sizeof(uint32_t)); else
      cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

void DrawCommands::drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, SceneGlobals::VisCamera viewId, Material::AlphaFunc func) {
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  auto b = std::lower_bound(ord.begin(), ord.end(), func, [](const DrawCmd* l, Material::AlphaFunc f){
    return l->alpha < f;
    });
  auto e = std::upper_bound(ord.begin(), ord.end(), func, [](Material::AlphaFunc f, const DrawCmd* r){
    return f < r->alpha;
    });

  auto& view = views[viewId];
  for(auto i=b; i!=e; ++i) {
    auto& cx = **i;
    if(cx.alpha!=func)
      continue;

    if(cx.maxPayload==0)
      continue;

    auto& desc = cx.isBindless() ? cx.desc : cx.descFr[fId];
    if(desc[viewId].isEmpty())
      continue;

    const RenderPipeline* pso = nullptr;
    switch(viewId) {
      case SceneGlobals::V_Shadow0:
      case SceneGlobals::V_Shadow1:
      case SceneGlobals::V_Vsm:
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
    if(cx.isMeshShader())
      cmd.dispatchMeshIndirect(view.indirectCmd, sizeof(IndirectCmd)*id + sizeof(uint32_t)); else
      cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

void DrawCommands::drawSwr(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; float znear; } push = {};
  push.firstMeshlet = 0;
  push.meshletCount = uint32_t(clusters.size());
  push.znear        = scene.znear;

  auto* pso = &Shaders::inst().swRendering;
  switch(Gothic::options().swRenderingPreset) {
    case 1: {
      cmd.setUniforms(*pso, swrDesc, &push, sizeof(push));
      //cmd.dispatch(10);
      cmd.dispatch(clusters.size());
      break;
      }
    case 2: {
      IVec2  tileSize = IVec2(128);
      int    tileX    = (scene.swMainImage->w()+tileSize.x-1)/tileSize.x;
      int    tileY    = (scene.swMainImage->h()+tileSize.y-1)/tileSize.y;
      cmd.setUniforms(*pso, swrDesc, &push, sizeof(push));
      cmd.dispatch(size_t(tileX), size_t(tileY)); //scene.swMainImage->size());
      break;
      }
    case 3: {
      cmd.setUniforms(*pso, swrDesc, &push, sizeof(push));
      cmd.dispatchThreads(scene.swMainImage->size());
      break;
      }
    }
  }
