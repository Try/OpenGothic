#include "rtscene.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/staticmesh.h"

using namespace Tempest;

RtScene::RtScene() {
  }

void RtScene::notifyTlas(const Material& mat, Category cat) const {
  if(cat!=Landscape && cat!=Static)
    return; // not supported
  if(mat.alpha!=Material::Solid && mat.alpha!=Material::AlphaTest)
    return; // not supported
  needToUpdate = true;
  }

bool RtScene::isUpdateRequired() const {
  return needToUpdate;
  }

uint32_t RtScene::aquireBucketId(const Material& mat, const StaticMesh& mesh) {
  for(size_t i=build.tex.size(); i>0; ) {
    --i;
    if(mat.tex!=build.tex[i] || &mesh.vbo!=build.vbo[i] || &mesh.ibo!=build.ibo[i])
      continue;
    return uint32_t(i);
    }
  build.tex.push_back(mat.tex);
  build.vbo.push_back(&mesh.vbo);
  build.ibo.push_back(&mesh.ibo);
  return uint32_t(build.tex.size()-1);
  }

void RtScene::addInstance(const Matrix4x4& pos, const AccelerationStructure& blas,
                          const Material& mat, const StaticMesh& mesh, size_t firstIndex, size_t iboLength,
                          Category cat) {
  if(cat!=Landscape && cat!=Static)
    return; // not supported
  if(mat.alpha!=Material::Solid && mat.alpha!=Material::AlphaTest && mat.alpha!=Material::Water)
    return; // not supported

  const uint32_t bucketId       = aquireBucketId(mat,mesh);
  const uint32_t firstPrimitive = uint32_t(firstIndex/3);

  RtObjectDesc desc = {};
  desc.instanceId     = bucketId;
  desc.firstPrimitive = firstPrimitive & 0x00FFFFFF; // 24 bit for primmitive + 8 for utility
  if(mat.alpha==Material::Solid)
    desc.bits |= 0x1;
  if(mat.alpha==Material::Water)
    desc.bits |= 0x2;

  RtInstance ix;
  ix.mat  = pos;
  ix.id   = uint32_t(build.rtDesc.size());
  ix.blas = &blas;
  if(mat.alpha==Material::Water)
    ix.flags = RtInstanceFlags::Opaque | RtInstanceFlags::CullDisable;
  else if(mat.alpha!=Material::Solid)
    ix.flags = RtInstanceFlags::NonOpaque;
  else
    ix.flags = RtInstanceFlags::Opaque | RtInstanceFlags::CullDisable;
  ix.flags = ix.flags | RtInstanceFlags::CullFlip;

  ix.mask = 0x1;
  if(mat.alpha==Material::Water)
    ix.mask |= 0x2;

  if(mat.alpha==Material::Solid && (cat==Landscape /*|| cat==Static*/)) {
    build.staticOpaque.geom  .push_back({mesh.vbo, mesh.ibo, firstIndex, iboLength});
    build.staticOpaque.rtDesc.push_back(desc);
    return;
    }
  if(mat.alpha==Material::AlphaTest && (cat==Landscape /*|| cat==Static*/)) {
    build.staticAt.geom  .push_back({mesh.vbo, mesh.ibo, firstIndex, iboLength});
    build.staticAt.rtDesc.push_back(desc);
    return;
    }

  build.inst.push_back(ix);
  build.rtDesc.push_back(desc);
  }

void RtScene::addInstance(const BuildBlas& ctx, Tempest::AccelerationStructure& blas, RtInstanceFlags flags) {
  auto& device = Resources::device();
  blas = device.blas(ctx.geom);

  Tempest::RtInstance ix;
  ix.mat   = Matrix4x4::mkIdentity();
  ix.id    = uint32_t(build.rtDesc.size());
  ix.flags = flags | RtInstanceFlags::CullFlip;
  ix.blas  = &blas;
  build.inst.push_back(ix);

  build.rtDesc.insert(build.rtDesc.end(), ctx.rtDesc.begin(), ctx.rtDesc.end());
  }

void RtScene::buildTlas() {
  needToUpdate = false;

  addInstance(build.staticOpaque, blasStaticOpaque, Tempest::RtInstanceFlags::Opaque | RtInstanceFlags::CullDisable);
  addInstance(build.staticAt, blasStaticAt, Tempest::RtInstanceFlags::NonOpaque);

  Resources::recycle(std::move(tex));
  Resources::recycle(std::move(vbo));
  Resources::recycle(std::move(ibo));
  Resources::recycle(std::move(rtDesc));
  Resources::recycle(std::move(tlas));

  auto& device = Resources::device();
  tex    = device.descriptors(build.tex);
  vbo    = device.descriptors(build.vbo);
  ibo    = device.descriptors(build.ibo);
  rtDesc = device.ssbo(build.rtDesc);
  tlas   = device.tlas(build.inst);

  build = Build();
  }

