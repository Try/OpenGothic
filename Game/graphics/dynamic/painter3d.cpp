#include "painter3d.h"

#include "graphics/bounds.h"

using namespace Tempest;

void Painter3d::Frustrum::make(const Matrix4x4& m) {
  float clip[16], t=0;
  std::copy(m.data(), m.data()+16, clip );

  f[0][0] = clip[ 3] - clip[ 0];
  f[0][1] = clip[ 7] - clip[ 4];
  f[0][2] = clip[11] - clip[ 8];
  f[0][3] = clip[15] - clip[12];

  t = std::sqrt(f[0][0]*f[0][0] + f[0][1]*f[0][1] + f[0][2]*f[0][2]);
  if(t==0)
    return clear();

  f[0][0] /= t;
  f[0][1] /= t;
  f[0][2] /= t;
  f[0][3] /= t;

  f[1][0] = clip[ 3] + clip[ 0];
  f[1][1] = clip[ 7] + clip[ 4];
  f[1][2] = clip[11] + clip[ 8];
  f[1][3] = clip[15] + clip[12];

  t = std::sqrt(f[1][0]*f[1][0] + f[1][1]*f[1][1] + f[1][2]*f[1][2]);
  if(t==0)
    return clear();

  f[1][0] /= t;
  f[1][1] /= t;
  f[1][2] /= t;
  f[1][3] /= t;

  f[2][0] = clip[ 3] + clip[ 1];
  f[2][1] = clip[ 7] + clip[ 5];
  f[2][2] = clip[11] + clip[ 9];
  f[2][3] = clip[15] + clip[13];

  t = std::sqrt(f[2][0]*f[2][0] + f[2][1]*f[2][1] + f[2][2]*f[2][2]);
  if(t==0)
    return clear();

  f[2][0] /= t;
  f[2][1] /= t;
  f[2][2] /= t;
  f[2][3] /= t;

  f[3][0] = clip[ 3] - clip[ 1];
  f[3][1] = clip[ 7] - clip[ 5];
  f[3][2] = clip[11] - clip[ 9];
  f[3][3] = clip[15] - clip[13];

  t = std::sqrt(f[3][0]*f[3][0] + f[3][1]*f[3][1] + f[3][2]*f[3][2]);
  if(t==0)
    return clear();

  f[3][0] /= t;
  f[3][1] /= t;
  f[3][2] /= t;
  f[3][3] /= t;

  f[4][0] = clip[ 3] - clip[ 2];
  f[4][1] = clip[ 7] - clip[ 6];
  f[4][2] = clip[11] - clip[10];
  f[4][3] = clip[15] - clip[14];

  t = std::sqrt(f[4][0]*f[4][0] + f[4][1]*f[4][1] + f[4][2]*f[4][2]);
  if(t==0)
    return clear();

  f[4][0] /= t;
  f[4][1] /= t;
  f[4][2] /= t;
  f[4][3] /= t;

  f[5][0] = clip[ 3] + clip[ 2];
  f[5][1] = clip[ 7] + clip[ 6];
  f[5][2] = clip[11] + clip[10];
  f[5][3] = clip[15] + clip[14];

  t = std::sqrt(f[5][0]*f[5][0] + f[5][1]*f[5][1] + f[5][2]*f[5][2]);
  if(t==0)
    return clear();

  f[5][0] /= t;
  f[5][1] /= t;
  f[5][2] /= t;
  f[5][3] /= t;
  }

void Painter3d::Frustrum::clear() {
  std::memset(f,0,sizeof(f));
  f[0][3] = -std::numeric_limits<float>::infinity();
  }

bool Painter3d::Frustrum::testPoint(float x, float y, float z) const {
  return testPoint(x,y,z,0);
  }

bool Painter3d::Frustrum::testPoint(float x, float y, float z, float R) const {
  for(size_t i=0; i<6; i++) {
    if(f[i][0]*x+f[i][1]*y+f[i][2]*z+f[i][3]<=-R)
      return false;
    }
  return true;
  }


Painter3d::PerFrame::PerFrame() {
  cmd.reserve(3);
  }


Painter3d::Painter3d(Device& device)
  :device(device) {
  }

Painter3d::~Painter3d() {
  }

void Painter3d::reset() {
  passId = 0;
  }

void Painter3d::setPass(const Tempest::FrameBuffer& fbo, uint8_t frameId) {
  current = &pf[frameId];
  current->cmd.resize(passId+1);
  auto rec = current->cmd[passId].startEncoding(device,fbo.layout(),fbo.w(),fbo.h());
  new(encBuf) Encoder<Tempest::CommandBuffer>(std::move(rec));
  enc = reinterpret_cast<Recorder*>(encBuf);
  }

void Painter3d::commit(Encoder<PrimaryCommandBuffer> &cmd) {
  enc->~Recorder();
  enc = nullptr;
  cmd.exec(current->cmd[passId]);
  passId++;
  }

void Painter3d::setFrustrum(const Matrix4x4& m) {
  frustrum.make(m);
  }

bool Painter3d::isVisible(const Bounds& b) const {
  return frustrum.testPoint(b.at.x,b.at.y,b.at.z, b.r);
  }

void Painter3d::draw(const RenderPipeline& pipeline, const Bounds& bbox,
                     const Uniforms& ubo, uint32_t offset,
                     const VertexBuffer<Vertex>& vbo, const IndexBuffer<uint32_t>& ibo) {
  if(!isVisible(bbox))
    return;
  enc->setUniforms(pipeline,ubo,1,&offset);
  enc->draw(vbo,ibo);
  }

void Painter3d::draw(const RenderPipeline& pipeline, const Bounds& bbox,
                     const Uniforms& ubo, uint32_t offset,
                     const VertexBuffer<Painter3d::VertexA>& vbo, const IndexBuffer<uint32_t>& ibo) {
  if(!isVisible(bbox))
    return;
  enc->setUniforms(pipeline,ubo,1,&offset);
  enc->draw(vbo,ibo);
  }
