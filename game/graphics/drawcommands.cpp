#include "drawcommands.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/animmesh.h"
#include "graphics/mesh/submesh/staticmesh.h"
#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/visualobjects.h"
#include "shaders.h"

#include "gothic.h"

using namespace Tempest;

static bool needtoReallocate(const StorageBuffer& b, const size_t desiredSz) {
  return b.byteSize()<desiredSz || b.byteSize()>=2*desiredSz;
  }


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


DrawCommands::DrawCommands(VisualObjects& owner, DrawBuckets& buckets, DrawClusters& clusters, const SceneGlobals& scene)
    : owner(owner), buckets(buckets), clusters(clusters), scene(scene), vsmSupported(Shaders::isVsmSupported()) {
  for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
    views[v].viewport = SceneGlobals::VisCamera(v);
    }

  if(vsmSupported) {
    Tempest::DispatchIndirectCommand cmd = {2000,1,1};
    vsmIndirectCmd = Resources::device().ssbo(&cmd, sizeof(cmd));
    }
  // vsmSwrImage = Resources::device().image2d(TextureFormat::R16,  4096, 4096);
  // vsmSwrImage = Resources::device().image2d(TextureFormat::R32U, 4096, 4096);
  }

DrawCommands::~DrawCommands() {
  }

bool DrawCommands::isViewEnabled(SceneGlobals::VisCamera viewport) const {
  if(viewport==SceneGlobals::V_Vsm && !vsmSupported)
    return false;
  if(viewport==SceneGlobals::V_Vsm && scene.vsmPageTbl->isEmpty())
    return false;
  if(viewport==SceneGlobals::V_Shadow0 && scene.shadowMap[0]->size()==Size(1,1))
    return false;
  if(viewport==SceneGlobals::V_Shadow1 && scene.shadowMap[1]->size()==Size(1,1))
    return false;
  return true;
  }

void DrawCommands::setBindings(Tempest::Encoder<CommandBuffer>& cmd, const DrawCmd& cx, SceneGlobals::VisCamera v) {
  static const DrawBuckets::Bucket nullBk;

  const auto  bId = cx.bucketId;
  const auto& bx  = cx.isBindless() ? nullBk : buckets.buckets()[bId];

  cmd.setBinding(L_Scene,    scene.uboGlobal[v]);
  cmd.setBinding(L_Payload,  views[v].visClusters);
  cmd.setBinding(L_Instance, owner.instanceSsbo());
  cmd.setBinding(L_Bucket,   buckets.ssbo());

  if(cx.isBindless()) {
    cmd.setBinding(L_Ibo, ibo);
    cmd.setBinding(L_Vbo, vbo);
    }
  else if(bx.staticMesh!=nullptr) {
    cmd.setBinding(L_Ibo, bx.staticMesh->ibo8);
    cmd.setBinding(L_Vbo, bx.staticMesh->vbo);
    }
  else {
    cmd.setBinding(L_Ibo, bx.animMesh->ibo8);
    cmd.setBinding(L_Vbo, bx.animMesh->vbo);
    }

  if(v==SceneGlobals::V_Main || cx.isTextureInShadowPass()) {
    if(cx.isBindless()) {
      cmd.setBinding(L_Diffuse, tex);
      }
    else if(bx.mat.hasFrameAnimation()) {
      uint64_t timeShift = 0;
      auto     frame     = size_t((timeShift+scene.tickCount)/bx.mat.texAniFPSInv);
      auto*    t         = bx.mat.frames[frame%bx.mat.frames.size()];
      cmd.setBinding(L_Diffuse, *t);
      }
    else {
      cmd.setBinding(L_Diffuse, *bx.mat.tex);
      }
    auto smp = SceneGlobals::isShadowView(v) ? Sampler::trillinear() : Sampler::anisotrophy();
    cmd.setBinding(L_Sampler, smp);
    }

  if(v==SceneGlobals::V_Main && cx.isShadowmapRequired()) {
    cmd.setBinding(L_Shadow0, *scene.shadowMap[0], Resources::shadowSampler());
    cmd.setBinding(L_Shadow1, *scene.shadowMap[1], Resources::shadowSampler());
    }

  if(cx.type==Morph && cx.isBindless()) {
    cmd.setBinding(L_MorphId,  morphId);
    cmd.setBinding(L_Morph,    morph);
    }
  else if(cx.type==Morph && bx.staticMesh!=nullptr) {
    cmd.setBinding(L_MorphId,  *bx.staticMesh->morph.index);
    cmd.setBinding(L_Morph,    *bx.staticMesh->morph.samples);
    }

  if(v==SceneGlobals::V_Main && cx.isSceneInfoRequired()) {
    cmd.setBinding(L_SceneClr, *scene.sceneColor, Sampler::bilinear(ClampMode::MirroredRepeat));
    cmd.setBinding(L_GDepth,   *scene.sceneDepth, Sampler::nearest (ClampMode::MirroredRepeat));
    }

  if(v==SceneGlobals::V_Vsm) {
    cmd.setBinding(L_CmdOffsets, views[v].indirectCmd);
    cmd.setBinding(L_VsmPages,   *scene.vsmPageList);
    cmd.setBinding(L_Lights,     *scene.lights);
    }
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

  DrawCmd cx;
  cx.pMain       = pMain;
  cx.pShadow     = pShadow;
  cx.pVsm        = pVsm;
  cx.pHiZ        = pHiZ;
  cx.bucketId    = bindless ? 0xFFFFFFFF : bucketId;
  cx.type        = type;
  cx.alpha       = m.alpha;
  cmd.push_back(std::move(cx));
  cmdDurtyBit = true;
  return ret;
  }

void DrawCommands::addClusters(uint16_t cmdId, uint32_t meshletCount) {
  cmdDurtyBit = true;
  cmd[cmdId].maxPayload += meshletCount;
  }

void DrawCommands::resetRendering() {
  for(auto& v:views) {
    Resources::recycle(std::move(v.indirectCmd));
    Resources::recycle(std::move(v.visClusters));
    Resources::recycle(std::move(v.vsmClusters));
    }
  cmdDurtyBit = true;
  }

void DrawCommands::commit(Encoder<CommandBuffer>& enc) {
  if(!cmdDurtyBit)
    return;
  cmdDurtyBit = false;

  ord.resize(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i)
    ord[i] = &cmd[i];
  std::sort(ord.begin(), ord.end(), [](const DrawCmd* l, const DrawCmd* r){
    return l->alpha < r->alpha;
    });

  size_t totalPayload = 0;
  bool   layChg       = false;
  for(auto& i:cmd) {
    layChg |= (i.firstPayload != uint32_t(totalPayload));
    i.firstPayload = uint32_t(totalPayload);
    totalPayload  += i.maxPayload;
    }

  totalPayload = (totalPayload + 0xFF) & ~size_t(0xFF);
  const size_t visClustersSz = totalPayload*sizeof(uint32_t)*4;

  auto& v      = views[SceneGlobals::V_Main];
  bool  cmdChg = needtoReallocate(v.indirectCmd, sizeof(IndirectCmd)*cmd.size());
  bool  visChg = needtoReallocate(v.visClusters, visClustersSz);
  this->maxPayload = totalPayload;

  if(!layChg && !visChg) {
    return;
    }

  std::vector<IndirectCmd> cx(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i) {
    auto mesh = cmd[i].isMeshShader();

    cx[i].vertexCount   = PackedMesh::MaxInd;
    cx[i].writeOffset   = cmd[i].firstPayload;
    cx[i].firstVertex   = mesh ? 1 : 0;
    cx[i].firstInstance = mesh ? 1 : 0;
    }

  auto& device = Resources::device();
  for(auto& v:views) {
    if(!isViewEnabled(v.viewport))
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
      }
    else if(layChg) {
      auto staging = device.ssbo(BufferHeap::Upload, cx.data(), sizeof(IndirectCmd)*cx.size());

      enc.setBinding(0, v.indirectCmd);
      enc.setBinding(1, staging);
      enc.setPipeline(Shaders::inst().copyBuf);
      enc.dispatchThreads(staging.byteSize()/sizeof(uint32_t));

      Resources::recycle(std::move(staging));
      }
    }
  }

void DrawCommands::updateBindlessArrays() {
  if(!Gothic::inst().options().doBindless)
    return;

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

  Resources::recycle(std::move(this->tex));
  Resources::recycle(std::move(this->vbo));
  Resources::recycle(std::move(this->ibo));
  Resources::recycle(std::move(this->morphId));
  Resources::recycle(std::move(this->morph));

  auto& device = Resources::device();
  this->tex     = device.descriptors(tex);
  this->vbo     = device.descriptors(vbo);
  this->ibo     = device.descriptors(ibo);
  this->morphId = device.descriptors(morphId);
  this->morph   = device.descriptors(morph);
  }

void DrawCommands::visibilityPass(Encoder<CommandBuffer>& cmd, int pass) {
  static bool freeze = false;
  if(freeze)
    return;

  cmd.setFramebuffer({});
  if(pass==0) {
    for(auto& v:views) {
      if(this->cmd.empty())
        continue;
      if(!isViewEnabled(v.viewport))
        continue;
      const uint32_t isMeshShader = (Gothic::options().doMeshShading ? 1 : 0);
      cmd.setBinding(T_Indirect, v.indirectCmd);
      cmd.setPushData(&isMeshShader, sizeof(isMeshShader));
      cmd.setPipeline(Shaders::inst().clusterInit);
      cmd.dispatchThreads(this->cmd.size());
      }
    }

  for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
    const auto viewport = SceneGlobals::VisCamera(v);
    if(viewport==SceneGlobals::V_HiZ && pass!=0)
      continue;
    if(viewport!=SceneGlobals::V_HiZ && pass==0)
      continue;
    if(viewport==SceneGlobals::V_Vsm)
      continue;
    if(!isViewEnabled(viewport))
      continue;

    struct Push { uint32_t firstMeshlet; uint32_t meshletCount; float znear; } push = {};
    push.firstMeshlet = 0;
    push.meshletCount = uint32_t(clusters.size());
    push.znear        = scene.znear;

    auto* pso = &Shaders::inst().visibilityPassSh;
    if(viewport==SceneGlobals::V_Main)
      pso = &Shaders::inst().visibilityPassHiZ;
    else if(viewport==SceneGlobals::V_HiZ)
      pso = &Shaders::inst().visibilityPassHiZCr;

    cmd.setBinding(T_Scene,    scene.uboGlobal[viewport]);
    cmd.setBinding(T_Payload,  views[viewport].visClusters);
    cmd.setBinding(T_Instance, owner.instanceSsbo());
    cmd.setBinding(T_Bucket,   buckets.ssbo());
    cmd.setBinding(T_Indirect, views[viewport].indirectCmd);
    cmd.setBinding(T_Clusters, clusters.ssbo());
    cmd.setBinding(T_HiZ,      *scene.hiZ);
    cmd.setPushData(push);
    cmd.setPipeline(*pso);
    cmd.dispatchThreads(push.meshletCount);
    }
  }

void DrawCommands::visibilityVsm(Encoder<CommandBuffer>& cmd) {
  auto& shaders = Shaders::inst();

  struct Push { uint32_t meshletCount; } push = {};
  push.meshletCount = uint32_t(clusters.size());

  auto& view = views[SceneGlobals::V_Vsm];
  cmd.setBinding(T_Scene,    scene.uboGlobal[SceneGlobals::V_Vsm]);
  cmd.setBinding(T_Payload,  view.vsmClusters); //unsorted clusters
  cmd.setBinding(T_Instance, owner.instanceSsbo());
  cmd.setBinding(T_Bucket,   buckets.ssbo());
  cmd.setBinding(T_Indirect, view.indirectCmd);
  cmd.setBinding(T_Clusters, clusters.ssbo());
  cmd.setBinding(T_Lights,   *scene.lights);
  cmd.setBinding(T_HiZ,      *scene.vsmPageHiZ);
  cmd.setBinding(T_VsmPages, *scene.vsmPageList);
  cmd.setBinding(9,          scene.vsmDbg);
  cmd.setPushData(&push, sizeof(push));
  cmd.setPipeline(shaders.vsmVisibilityPass);
  cmd.dispatchThreads(push.meshletCount, size_t(scene.vsmPageTbl->d() + 1));

  cmd.setBinding(1, view.vsmClusters);
  cmd.setBinding(2, view.visClusters);
  cmd.setBinding(3, view.indirectCmd);
  cmd.setBinding(4, *scene.vsmPageList);
  cmd.setBinding(5, vsmIndirectCmd);
  cmd.setPipeline(shaders.vsmPackDraw0);
  cmd.dispatch(1);

  cmd.setBinding(1, view.vsmClusters);
  cmd.setBinding(2, view.visClusters);
  cmd.setBinding(3, view.indirectCmd);
  cmd.setBinding(4, *scene.vsmPageList);
  cmd.setPipeline(shaders.vsmPackDraw1);
  cmd.dispatch(8096); // TODO: indirect
  // cmd.dispatchIndirect(vsmIndirectCmd, 0);
  }

void DrawCommands::drawVsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  // return;
  struct Push { uint32_t commandId; } push = {};

  auto  viewId = SceneGlobals::V_Vsm;
  auto& view   = views[viewId];

  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.alpha!=Material::Solid && cx.alpha!=Material::AlphaTest)
      continue;
    if(cx.pVsm==nullptr)
      continue;

    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.commandId = uint32_t(id);

    // cmd.setUniforms(*cx.pVsm, desc[viewId], &push, sizeof(push));
    setBindings(cmd, cx, viewId);
    cmd.setPushData(push);
    cmd.setPipeline(*cx.pVsm);
    if(cx.isMeshShader())
      cmd.dispatchMeshIndirect(view.indirectCmd, sizeof(IndirectCmd)*id + sizeof(uint32_t)); else
      cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }

  if(false) {
    struct Push { uint32_t meshletCount; } push = {};
    push.meshletCount = uint32_t(clusters.size());

    cmd.setFramebuffer({});
    cmd.setBinding(0, *scene.vsmPageData);
    cmd.setBinding(1, scene.uboGlobal[SceneGlobals::V_Vsm]);
    cmd.setBinding(2, *scene.vsmPageList);
    cmd.setBinding(3, clusters.ssbo());
    cmd.setBinding(4, owner.instanceSsbo());
    cmd.setBinding(5, ibo);
    cmd.setBinding(6, vbo);
    cmd.setBinding(7, tex);
    cmd.setBinding(8, Sampler::bilinear());
    cmd.setPushData(&push, sizeof(push));
    cmd.setPipeline(Shaders::inst().vsmRendering);
    // const auto sz = Shaders::inst().vsmRendering.workGroupSize();
    cmd.dispatch(1024u);
    }
  }

void DrawCommands::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  // return;
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  auto  viewId = SceneGlobals::V_HiZ;
  auto& view   = views[viewId];

  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.alpha!=Material::Solid && cx.alpha!=Material::AlphaTest)
      continue;
    if(cx.type!=Landscape && cx.type!=Static)
      continue;
    if(cx.pHiZ==nullptr)
      continue;

    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstPayload;
    push.meshletCount = cx.maxPayload;

    setBindings(cmd, cx, viewId);
    cmd.setPushData(push);
    cmd.setPipeline(*cx.pHiZ);
    if(cx.isMeshShader())
      cmd.dispatchMeshIndirect(view.indirectCmd, sizeof(IndirectCmd)*id + sizeof(uint32_t)); else
      cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

void DrawCommands::drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, SceneGlobals::VisCamera viewId, Material::AlphaFunc func) {
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

    setBindings(cmd, cx, viewId);
    cmd.setPushData(push);
    cmd.setPipeline(*pso);
    if(cx.isMeshShader())
      cmd.dispatchMeshIndirect(view.indirectCmd, sizeof(IndirectCmd)*id + sizeof(uint32_t)); else
      cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

void DrawCommands::drawSwr(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; float znear; } push = {};
  push.firstMeshlet = 0;
  push.meshletCount = uint32_t(clusters.size());
  push.znear        = scene.znear;

  cmd.setBinding(0, *scene.swMainImage);
  cmd.setBinding(1, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(2, *scene.gbufNormals);
  cmd.setBinding(3, *scene.zbuffer);
  cmd.setBinding(4, clusters.ssbo());
  cmd.setBinding(5, ibo);
  cmd.setBinding(6, vbo);
  cmd.setBinding(7, tex);
  cmd.setBinding(8, Sampler::bilinear());

  auto* pso = &Shaders::inst().swRendering;
  switch(Gothic::options().swRenderingPreset) {
    case 1: {
      cmd.setPushData(&push, sizeof(push));
      cmd.setPipeline(*pso);
      //cmd.dispatch(10);
      cmd.dispatch(clusters.size());
      break;
      }
    case 2: {
      IVec2  tileSize = IVec2(128);
      int    tileX    = (scene.swMainImage->w()+tileSize.x-1)/tileSize.x;
      int    tileY    = (scene.swMainImage->h()+tileSize.y-1)/tileSize.y;
      cmd.setPushData(&push, sizeof(push));
      cmd.setPipeline(*pso);
      cmd.dispatch(size_t(tileX), size_t(tileY)); //scene.swMainImage->size());
      break;
      }
    case 3: {
      cmd.setBinding(9,  *scene.lights);
      cmd.setBinding(10, owner.instanceSsbo());
      cmd.setPushData(&push, sizeof(push));
      cmd.setPipeline(*pso);
      cmd.dispatchThreads(scene.swMainImage->size());
      break;
      }
    }
  }

static Size tileCount(Size sz, int s) {
  sz.w = (sz.w+s-1)/s;
  sz.h = (sz.h+s-1)/s;
  return sz;
  }

static Size roundUp(Size sz, int align) {
  sz.w = (sz.w+align-1)/align;
  sz.h = (sz.h+align-1)/align;

  sz.w *= align;
  sz.h *= align;
  return sz;
  }

void DrawCommands::drawRtsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  const int RTSM_BIN_SIZE   = 32;
  const int RTSM_SMALL_TILE = 32;
  const int RTSM_LARGE_TILE = 128;

  auto& device   = Resources::device();
  auto& shaders  = Shaders::inst();
  auto& sceneUbo = scene.uboGlobal[SceneGlobals::V_Vsm];

  if(rtsmPages.isEmpty()) {
    rtsmPages = device.image3d(TextureFormat::R32U, 32, 32, 16);
    }

  if(rtsmLargeTile.size()!=tileCount(scene.zbuffer->size(), RTSM_LARGE_TILE)) {
    auto sz = tileCount(scene.zbuffer->size(), RTSM_LARGE_TILE);
    Resources::recycle(std::move(rtsmLargeTile));
    rtsmLargeTile = device.image2d(TextureFormat::RG32U, uint32_t(sz.w), uint32_t(sz.h));
    }
  if(rtsmSmallTile.size()!=tileCount(scene.zbuffer->size(), RTSM_SMALL_TILE)) {
    auto sz = tileCount(scene.zbuffer->size(), RTSM_SMALL_TILE);
    Resources::recycle(std::move(rtsmSmallTile));
    rtsmSmallTile = device.image2d(TextureFormat::RG32U, uint32_t(sz.w), uint32_t(sz.h));
    }
  if(rtsmPrimBins.size()!=roundUp(scene.zbuffer->size(), RTSM_BIN_SIZE)) {
    auto sz = tileCount(scene.zbuffer->size(), RTSM_BIN_SIZE);
    Resources::recycle(std::move(rtsmPrimBins));
    // NOTE: need to improve engine api to accept size
    rtsmPrimBins = device.image2d(TextureFormat::RG32U, uint32_t(sz.w), uint32_t(sz.h));
    }

  const size_t clusterCnt = maxPayload;
  const size_t clusterSz  = shaders.rtsmClear.sizeofBuffer(1, clusterCnt);
  if(rtsmVisList.byteSize()!=clusterSz) {
    Resources::recycle(std::move(rtsmVisList));
    rtsmVisList = device.ssbo(nullptr, clusterSz);
    }

  if(rtsmPosList.isEmpty()) {
    rtsmPosList = device.ssbo(Tempest::Uninitialized, 64*1024*1024); // arbitrary
    }

  {
    // clear
    cmd.setBinding(0, rtsmPages);
    cmd.setBinding(1, rtsmVisList);
    cmd.setBinding(2, rtsmPosList);

    cmd.setPipeline(shaders.rtsmClear);
    cmd.dispatchThreads(size_t(rtsmPages.w()), size_t(rtsmPages.h()), size_t(rtsmPages.d()));
  }

  {
    // cull
    struct Push { uint32_t meshletCount; } push = {};
    push.meshletCount = uint32_t(clusters.size());
    cmd.setPushData(push);

    cmd.setBinding(0, *scene.rtsmImage);
    cmd.setBinding(1, sceneUbo);
    cmd.setBinding(2, *scene.gbufDiffuse);
    cmd.setBinding(3, *scene.gbufNormals);
    cmd.setBinding(4, *scene.zbuffer);
    cmd.setBinding(5, clusters.ssbo());
    cmd.setBinding(6, rtsmVisList);
    cmd.setBinding(7, rtsmPages);

    cmd.setPipeline(shaders.rtsmPages);
    cmd.dispatchThreads(scene.zbuffer->size());

    cmd.setBinding(0, rtsmPages);
    cmd.setPipeline(shaders.rtsmHiZ);
    cmd.dispatch(1);

    cmd.setBinding(7, rtsmPosList);
    cmd.setPipeline(shaders.rtsmCulling);
    cmd.dispatchThreads(push.meshletCount);
  }

  {
    // position
    cmd.setBinding(0, rtsmPosList);
    cmd.setBinding(1, sceneUbo);
    cmd.setBinding(2, rtsmVisList);

    cmd.setBinding(5, clusters.ssbo());
    cmd.setBinding(6, owner.instanceSsbo());
    cmd.setBinding(7, buckets.ssbo());
    cmd.setBinding(8, ibo);
    cmd.setBinding(9, vbo);
    cmd.setBinding(10, morphId);
    cmd.setBinding(11, morph);

    cmd.setPipeline(shaders.rtsmPosition);
    cmd.dispatchIndirect(rtsmVisList,0);
  }

  {
    // large tiles
    cmd.setBinding(0, *scene.rtsmImage);
    cmd.setBinding(1, sceneUbo);
    cmd.setBinding(2, *scene.gbufNormals);
    cmd.setBinding(3, *scene.zbuffer);
    cmd.setBinding(4, rtsmVisList);
    cmd.setBinding(5, rtsmPosList);
    cmd.setBinding(6, rtsmLargeTile);
    cmd.setBinding(7, rtsmSmallTile);

    cmd.setPipeline(shaders.rtsmLargeTiles);
    cmd.dispatch(uint32_t(rtsmLargeTile.w()), uint32_t(rtsmLargeTile.h()));

    // small tiles
    cmd.setPipeline(shaders.rtsmSmallTiles);
    cmd.dispatch(uint32_t(rtsmSmallTile.w()), uint32_t(rtsmSmallTile.h()));
  }

  {
    // in-tile culling
    cmd.setBinding(0, *scene.rtsmImage);
    cmd.setBinding(1, sceneUbo);
    cmd.setBinding(2, *scene.gbufNormals);
    cmd.setBinding(3, *scene.zbuffer);
    cmd.setBinding(4, rtsmSmallTile);
    // cmd.setBinding(4, rtsmLargeTile);
    cmd.setBinding(5, rtsmPosList);
    cmd.setBinding(6, rtsmPrimBins);
    cmd.setBinding(7, *scene.rtsmDbg);

    cmd.setPipeline(shaders.rtsmTileCulling);
    cmd.dispatch(uint32_t(rtsmPrimBins.w()), uint32_t(rtsmPrimBins.h()));
    //cmd.dispatchThreads(scene.rtsmImage->size());
  }

  static bool reference = false;
  if(!reference) {
    cmd.setBinding(0, *scene.rtsmImage);
    cmd.setBinding(1, sceneUbo);
    cmd.setBinding(2, *scene.gbufNormals);
    cmd.setBinding(3, *scene.zbuffer);
    cmd.setBinding(4, rtsmPrimBins);
    cmd.setBinding(5, rtsmPosList);

    cmd.setBinding(7, tex);
    cmd.setBinding(8, Sampler::trillinear());

    cmd.setPipeline(shaders.rtsmRaster);
    cmd.dispatchThreads(scene.rtsmImage->size());
    } else {
    struct Push { uint32_t meshletCount; } push = {};
    push.meshletCount = uint32_t(clusters.size());
    cmd.setPushData(push);

    cmd.setBinding(0, *scene.rtsmImage);
    cmd.setBinding(1, sceneUbo);
    cmd.setBinding(2, *scene.gbufNormals);
    cmd.setBinding(3, *scene.zbuffer);
    cmd.setBinding(4, rtsmVisList);

    cmd.setBinding(5, clusters.ssbo());
    cmd.setBinding(6, owner.instanceSsbo());
    cmd.setBinding(7, ibo);
    cmd.setBinding(8, vbo);
    cmd.setBinding(9, tex);
    cmd.setBinding(10, Sampler::bilinear());

    cmd.setPipeline(shaders.rtsmRendering);
    cmd.dispatchThreads(scene.rtsmImage->size());
    }
  }
