#include "painter3d.h"

#include "graphics/bounds.h"
#include "graphics/light.h"

using namespace Tempest;

Painter3d::Painter3d(Tempest::Encoder<CommandBuffer>& enc)
  :enc(enc) {
  }

Painter3d::~Painter3d() {
  }

void Painter3d::setFrustrum(const Matrix4x4& m) {
  frustrum.make(m);
  }

bool Painter3d::isVisible(const Bounds& b) const {
  return frustrum.testPoint(b.midTr.x,b.midTr.y,b.midTr.z, b.r);
  }

void Painter3d::setViewport(int x, int y, int w, int h) {
  enc.setViewport(x,y,w,h);
  }

void Painter3d::setUniforms(const RenderPipeline& pipeline, const void* b, size_t sz) {
  enc.setUniforms(pipeline,b,sz);
  }

void Painter3d::draw(const RenderPipeline& pipeline, const Uniforms& ubo,
                     const Tempest::VertexBuffer<Painter3d::Vertex>& vbo, const Tempest::IndexBuffer<uint32_t>& ibo) {
  enc.setUniforms(pipeline,ubo);
  enc.draw(vbo,ibo);
  }

void Painter3d::draw(const RenderPipeline& pipeline, const Uniforms& ubo,
                     const Tempest::VertexBuffer<Painter3d::VertexA>& vbo, const Tempest::IndexBuffer<uint32_t>& ibo) {
  enc.setUniforms(pipeline,ubo);
  enc.draw(vbo,ibo);
  }

void Painter3d::draw(const RenderPipeline& pipeline, const Uniforms& ubo, const Tempest::VertexBuffer<VertexFsq>& vbo) {
  enc.setUniforms(pipeline,ubo);
  enc.draw(vbo);
  }

void Painter3d::draw(const RenderPipeline& pipeline, const Uniforms& ubo, const Tempest::VertexBuffer<Painter3d::Vertex>& vbo) {
  enc.setUniforms(pipeline,ubo);
  enc.draw(vbo);
  }
