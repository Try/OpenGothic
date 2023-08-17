#include "rtscene.h"

#include "graphics/mesh/submesh/staticmesh.h"

using namespace Tempest;

RtScene::RtScene() {
  }

void RtScene::addInstance(const Matrix4x4& pos, const AccelerationStructure& blas,
                          const Material& mat, const StaticMesh& mesh, size_t fistIndex) {
  if(mat.tex!=build.tex.back() || &mesh.vbo!=build.vbo.back() || &mesh.ibo!=build.ibo.back() ||
     uint32_t(fistIndex/3)!=build.iboOff.back()) {
    build.tex   .push_back(mat.tex);
    build.vbo   .push_back(&mesh.vbo);
    build.ibo   .push_back(&mesh.ibo);
    build.iboOff.push_back(uint32_t(fistIndex/3));
    }

  RtInstance ix;
  ix.mat  = pos;
  ix.id   = uint32_t(build.tex.size()-1);
  ix.blas = &blas;
  if(mat.alpha!=Material::Solid)
    ix.flags = Tempest::RtInstanceFlags::NonOpaque;
  build.inst.push_back(ix);
  }

void RtScene::addInstance(const Tempest::AccelerationStructure& blas) {
  build.tex   .push_back(&Resources::fallbackBlack());
  build.vbo   .push_back(nullptr);
  build.ibo   .push_back(nullptr);
  build.iboOff.push_back(0);

  Tempest::RtInstance ix;
  ix.mat  = Matrix4x4::mkIdentity();
  ix.blas = &blas;
  build.inst.push_back(ix);
  }

void RtScene::buildTlas() {
  auto& device = Resources::device();
  device.waitIdle();

  tex       = std::move(build.tex);
  vbo       = std::move(build.vbo);
  ibo       = std::move(build.ibo);
  iboOffset = device.ssbo(build.iboOff);
  tlas      = device.tlas(build.inst);

  build = Build();
  }
