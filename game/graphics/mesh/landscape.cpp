#include "landscape.h"

#include <Tempest/Log>
#include <cstddef>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/shaders.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(VisualObjects& visual, const PackedMesh &packed)
  :mesh(packed) {
  auto& device = Resources::device();

  auto meshletDescCpu = packed.meshletBounds;
  blocks.reserve(packed.subMeshes.size());
  for(size_t i=0; i<packed.subMeshes.size(); ++i) {
    auto& sub      = packed.subMeshes[i];
    auto  id       = uint32_t(sub.iboOffset/PackedMesh::MaxInd);
    auto  len      = uint32_t(sub.iboLength/PackedMesh::MaxInd);
    auto  material = Resources::loadMaterial(sub.material,true);

    if(material.alpha==Material::AdditiveLight || sub.iboLength==0) {
      for(size_t r=0; r<len; ++r) {
        meshletDescCpu[id+r].r        = -1.f;
        meshletDescCpu[id+r].bucketId = 0;
        }
      continue;
      }

    if(Gothic::options().doRayQuery) {
      mesh.sub[i].blas = device.blas(mesh.vbo,mesh.ibo,sub.iboOffset,sub.iboLength);
      }

    auto bkId = this->bucketId(material);
    if(bkId!=uint32_t(-1)) {
      cmd[bkId].maxClusters += len;
      for(size_t r=0; r<len; ++r)
        meshletDescCpu[id+r].bucketId = bkId;
      continue;
      }

    // non-GDR
    for(size_t r=0; r<len; ++r) {
      meshletDescCpu[id+r].r        = -1.f;
      meshletDescCpu[id+r].bucketId = 0;
      }
    Bounds bbox;
    bbox.assign(packed.vertices,packed.indices,sub.iboOffset,sub.iboLength);

    Block b;
    b.mesh = visual.get(mesh,material,sub.iboOffset,sub.iboLength,meshletDesc,bbox,ObjectsBucket::Landscape);
    b.mesh.setObjMatrix(Matrix4x4::mkIdentity());
    blocks.emplace_back(std::move(b));
    }

  meshletDesc = Resources::ssbo(meshletDescCpu.data(),meshletDescCpu.size()*sizeof(meshletDescCpu[0]));

  enum UboLinkpackage : uint8_t {
    L_Scene    = 0,
    L_Matrix   = 1,
    L_MeshDesc = L_Matrix,
    L_Bucket   = 2,
    L_Ibo      = 3,
    L_Vbo      = 4,
    L_Diffuse  = 5,
    L_Shadow0  = 6,
    L_Shadow1  = 7,
    L_MorphId  = 8,
    L_Morph    = 9,
    L_Pfx      = L_MorphId,
    L_SceneClr = 10,
    L_GDepth   = 11,
    L_HiZ      = 12,
    L_SkyLut   = 13,
    L_Payload  = 14,
    L_Sampler  = 15,
    };

  clusterTotal = uint32_t(mesh.ibo.size() + PackedMesh::MaxInd - 1)/PackedMesh::MaxInd;
  visClusters  = device.ssbo(nullptr, clusterTotal*sizeof(uint32_t));

  uint32_t writeOffset = 0;
  std::vector<IndirectCmd> cx(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i) {
    cx[i].vertexCount = PackedMesh::MaxInd;
    cx[i].writeOffset = writeOffset;
    writeOffset += cmd[i].maxClusters;
    }
  indirectCmd = device.ssbo(cx.data(), sizeof(IndirectCmd)*cx.size());

  descInit = device.descriptors(Shaders::inst().clusterInit);
  descInit.set(L_Bucket,  indirectCmd);
  descInit.set(L_Payload, visClusters);

  descTask = device.descriptors(Shaders::inst().clusterTask);
  descTask.set(L_MeshDesc, meshletDesc);
  descTask.set(L_Bucket,   indirectCmd);
  descTask.set(L_Payload,  visClusters);

  std::vector<const Tempest::Texture2d*> tex;
  for(auto& i:packed.subMeshes) {
    auto material = Resources::loadMaterial(i.material,true);
    tex.push_back(material.tex);
    }
  for(auto& i:cmd) {
    i.desc.set(L_Ibo,      mesh.ibo8);
    i.desc.set(L_Vbo,      mesh.vbo);
    i.desc.set(L_Diffuse,  tex);
    i.desc.set(L_Bucket,   indirectCmd);
    i.desc.set(L_Payload,  visClusters);
    i.desc.set(L_Sampler,  Sampler::anisotrophy());
    }
  }

void Landscape::prepareUniforms(const SceneGlobals& sc) {
  descTask.set(0,   sc.uboGlobal[SceneGlobals::V_Main]);
  descTask.set(12, *sc.hiZ);

  for(auto& i:cmd) {
    i.desc.set(0, sc.uboGlobal[SceneGlobals::V_Main]);
    }
  }

void Landscape::visibilityPass(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t frameId) {
  cmd.setUniforms(Shaders::inst().clusterInit, descInit);
  cmd.dispatchThreads(this->cmd.size());

  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};
  push.firstMeshlet = 0;
  push.meshletCount = clusterTotal;

  cmd.setUniforms(Shaders::inst().clusterTask, descTask, &push, sizeof(push));
  cmd.dispatchThreads(clusterTotal);
  }

void Landscape::drawGBuffer(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t frameId) {
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {0, clusterTotal};

  for(size_t i=0; i<this->cmd.size(); ++i) {
    auto& cx = this->cmd[i];
    cmd.setUniforms(*cx.pso, cx.desc, &push, sizeof(push));
    cmd.drawIndirect(indirectCmd, sizeof(IndirectCmd)*i);
    push.firstMeshlet += cx.maxClusters;
    }
  // cmd.dispatchMeshIndirect(visClusters, offsetof(PayloadHdr, instanceCount));
  }

const RenderPipeline* Landscape::pipeline(const Material& m) {
  switch (m.alpha) {
    case Material::Solid:
      return &Shaders::inst().clusterGBuf;
    case Material::AlphaTest:
      return &Shaders::inst().clusterGBufAt;
    case Material::Water:
    case Material::Ghost:
    case Material::Multiply:
    case Material::Multiply2:
    case Material::Transparent:
    case Material::AdditiveLight:
      break;
    }
  return nullptr;
  }

uint32_t Landscape::bucketId(const Material& m) {
  auto pMain = pipeline(m);
  if(pMain==nullptr)
    return uint32_t(-1);

  const bool bindless = true;

  for(size_t i=0; i<cmd.size(); ++i) {
    if(cmd[i].pso!=pMain)
      continue;
    if(!bindless && cmd[i].tex != m.tex)
      continue;
    return uint32_t(i);
    }

  auto ret = uint32_t(cmd.size());

  auto& device = Resources::device();
  BlockCmd cx;
  cx.pso         = pMain;
  cx.tex         = m.tex;
  cx.desc        = device.descriptors(*cx.pso);
  cx.maxClusters = 0;
  cx.alpha       = m.alpha;
  cmd.push_back(std::move(cx));
  return ret;
  }
