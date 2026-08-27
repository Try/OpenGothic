#include "im3dgizmo.h"

#include <Tempest/Attachment>
#include <Tempest/Device>
#include <Tempest/Encoder>
#include <Tempest/RenderPipeline>
#include <Tempest/RenderState>
#include <Tempest/VertexBuffer>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <im3d.h>

#include "camera.h"
#include "resources.h"
#include "shader.h"

namespace {

struct GpuVertex {
  float positionSize[4];
  float color[4];
  };

struct Push {
  Tempest::Matrix4x4 viewProject;
  Tempest::Vec2      viewport;
  };

static_assert(sizeof(GpuVertex)==sizeof(float)*8);
static_assert(sizeof(Push)==sizeof(float)*18);

GpuVertex toGpuVertex(const Im3d::VertexData& src) {
  GpuVertex ret;
  ret.positionSize[0] = src.m_positionSize.x;
  ret.positionSize[1] = src.m_positionSize.y;
  ret.positionSize[2] = src.m_positionSize.z;
  ret.positionSize[3] = src.m_positionSize.w;
  ret.color[0] = src.m_color.getR();
  ret.color[1] = src.m_color.getG();
  ret.color[2] = src.m_color.getB();
  ret.color[3] = src.m_color.getA();
  return ret;
  }

Tempest::RenderState overlayState() {
  Tempest::RenderState state;
  state.setBlendSource(Tempest::RenderState::BlendMode::SrcAlpha);
  state.setBlendDest(Tempest::RenderState::BlendMode::OneMinusSrcAlpha);
  state.setZTestMode(Tempest::RenderState::ZTestMode::Always);
  state.setZWriteEnabled(false);
  state.setCullFaceMode(Tempest::RenderState::CullMode::NoCull);
  return state;
  }

Tempest::Shader loadShader(Tempest::Device& device, const char* name) {
  const auto src = GothicShader::get(name);
  return device.shader(src.data,src.len);
  }

}

struct Im3dGizmo::Impl {
  enum class Primitive : uint8_t {
    Points,
    Lines,
    Triangles,
    };

  struct Draw {
    Primitive primitive = Primitive::Triangles;
    uint32_t  offset = 0;
    uint32_t  count  = 0;
    };

  struct FrameBuffer {
    Tempest::VertexBuffer<GpuVertex> vertices;
    size_t capacity = 0;
    };

  std::vector<GpuVertex> cpuVertices;
  std::vector<Draw>      draws;
  Push                   push = {};
  std::array<FrameBuffer,Resources::MaxFramesInFlight> gpu;

  Tempest::RenderPipeline points;
  Tempest::RenderPipeline lines;
  Tempest::RenderPipeline triangles;
  bool pipelinesReady = false;

  void ensurePipelines() {
    if(pipelinesReady)
      return;

    auto& device = Resources::device();
    const auto state = overlayState();
    points = device.pipeline(Tempest::Points,state,
                             loadShader(device,"im3d_points.vert.sprv"),
                             loadShader(device,"im3d_points.frag.sprv"));
    lines = device.pipeline(Tempest::Lines,state,
                            loadShader(device,"im3d_lines.vert.sprv"),
                            loadShader(device,"im3d_lines.geom.sprv"),
                            loadShader(device,"im3d_lines.frag.sprv"));
    triangles = device.pipeline(Tempest::Triangles,state,
                                loadShader(device,"im3d_triangles.vert.sprv"),
                                loadShader(device,"im3d_triangles.frag.sprv"));
    pipelinesReady = true;
    }

  void copyDrawLists() {
    cpuVertices.clear();
    draws.clear();

    const auto* lists = Im3d::GetDrawLists();
    const auto count = Im3d::GetDrawListCount();
    for(Im3d::U32 listId=0; listId<count; ++listId) {
      const auto& src = lists[listId];
      if(src.m_vertexCount==0)
        continue;

      Draw draw;
      draw.offset = uint32_t(cpuVertices.size());
      draw.count  = src.m_vertexCount;
      switch(src.m_primType) {
        case Im3d::DrawPrimitive_Points:    draw.primitive=Primitive::Points;    break;
        case Im3d::DrawPrimitive_Lines:     draw.primitive=Primitive::Lines;     break;
        case Im3d::DrawPrimitive_Triangles: draw.primitive=Primitive::Triangles; break;
        default: continue;
        }

      cpuVertices.reserve(cpuVertices.size()+src.m_vertexCount);
      for(Im3d::U32 i=0; i<src.m_vertexCount; ++i)
        cpuVertices.push_back(toGpuVertex(src.m_vertexData[i]));
      draws.push_back(draw);
      }
    }

  void upload(uint8_t frameId) {
    auto& frame = gpu[frameId%gpu.size()];
    if(cpuVertices.empty())
      return;

    if(frame.capacity<cpuVertices.size()) {
      size_t capacity = std::max<size_t>(256,frame.capacity);
      while(capacity<cpuVertices.size())
        capacity *= 2;
      auto initial = cpuVertices;
      initial.resize(capacity);
      frame.vertices = Resources::device().vbo(Tempest::BufferHeap::Upload,initial);
      frame.capacity = capacity;
      }
    else {
      frame.vertices.update(cpuVertices.data(),0,cpuVertices.size()*sizeof(GpuVertex));
      }
    }
  };

Im3dGizmo::Im3dGizmo()
  :impl(new Impl()) {
  }

Im3dGizmo::~Im3dGizmo() = default;

void Im3dGizmo::setMode(Mode mode) {
  currentMode = mode;
  }

Im3dGizmo::Mode Im3dGizmo::mode() const {
  return currentMode;
  }

bool Im3dGizmo::isHovered() const {
  return Im3d::GetHotId()!=Im3d::Id_Invalid;
  }

bool Im3dGizmo::isActive() const {
  return Im3d::GetActiveId()!=Im3d::Id_Invalid;
  }

bool Im3dGizmo::prepare(const Camera& camera,
                        const Tempest::Vec3& cursorRayOrigin, const Tempest::Vec3& cursorRayDirection,
                        bool mouseDown, int width, int height, Tempest::Matrix4x4* transform) {
  const auto listener = camera.listenerPosition();
  const auto projection = camera.projective();

  auto& appData = Im3d::GetAppData();
  appData.m_keyDown[Im3d::Mouse_Left] = mouseDown;
  appData.m_cursorRayOrigin = Im3d::Vec3(cursorRayOrigin.x,cursorRayOrigin.y,cursorRayOrigin.z);
  appData.m_cursorRayDirection = Im3d::Vec3(cursorRayDirection.x,cursorRayDirection.y,cursorRayDirection.z);
  appData.m_worldUp = Im3d::Vec3(0,1,0);
  appData.m_viewOrigin = Im3d::Vec3(listener.pos.x,listener.pos.y,listener.pos.z);
  appData.m_viewDirection = Im3d::Vec3(listener.front.x,listener.front.y,listener.front.z);
  appData.m_viewportSize = Im3d::Vec2(float(width),float(height));
  appData.m_projScaleY = 2.f/std::max(std::abs(projection.at(1,1)),1e-7f);
  appData.m_projOrtho = false;
  appData.m_deltaTime = 1.f/60.f;

  Im3d::NewFrame();
  switch(currentMode) {
    case Mode::Translation: Im3d::GetContext().m_gizmoMode=Im3d::GizmoMode_Translation; break;
    case Mode::Rotation:    Im3d::GetContext().m_gizmoMode=Im3d::GizmoMode_Rotation;    break;
    case Mode::Scale:       Im3d::GetContext().m_gizmoMode=Im3d::GizmoMode_Scale;       break;
    }
  Im3d::GetContext().m_gizmoLocal=false;

  const bool changed = transform!=nullptr && Im3d::Gizmo("Spacer.WorldEditor.Selection",(*transform)[0]);
  Im3d::EndFrame();

  impl->push.viewProject = camera.viewProj();
  impl->push.viewport = Tempest::Vec2(float(width),float(height));
  impl->copyDrawLists();
  return changed;
  }

void Im3dGizmo::render(Tempest::Encoder<Tempest::CommandBuffer>& cmd,
                       Tempest::Attachment& target, uint8_t frameId) {
  if(impl->cpuVertices.empty())
    return;

  impl->ensurePipelines();
  impl->upload(frameId);
  auto& vertices = impl->gpu[frameId%impl->gpu.size()].vertices;

  cmd.setFramebuffer({{target,Tempest::Preserve,Tempest::Preserve}});
  cmd.setViewport(0,0,target.w(),target.h());

  for(const auto& draw:impl->draws) {
    switch(draw.primitive) {
      case Impl::Primitive::Points:    cmd.setPipeline(impl->points);    break;
      case Impl::Primitive::Lines:     cmd.setPipeline(impl->lines);     break;
      case Impl::Primitive::Triangles: cmd.setPipeline(impl->triangles); break;
      }
    cmd.setPushData(impl->push);
    cmd.draw(vertices,draw.offset,draw.count);
    }
  }
