#include "selectionoutline.h"

#include <Tempest/Attachment>
#include <Tempest/Device>
#include <Tempest/Encoder>
#include <Tempest/RenderPipeline>
#include <Tempest/RenderState>
#include <Tempest/ZBuffer>

#include <vector>

#include "camera.h"
#include "resources.h"
#include "shader.h"
#include "ui/objects/worldedit.h"

namespace {

struct MaskPush {
  Tempest::Matrix4x4 model;
  Tempest::Matrix4x4 viewProject;
  Tempest::Vec4      params;
  };

struct CompositePush {
  Tempest::Vec4 color;
  Tempest::Vec2 viewport;
  float         radius;
  float         padding;
  };

static_assert(sizeof(MaskPush)==sizeof(float)*36);
static_assert(sizeof(CompositePush)==sizeof(float)*8);

Tempest::Shader loadShader(Tempest::Device& device, const char* name) {
  const auto src = GothicShader::get(name);
  return device.shader(src.data,src.len);
  }

}

struct SelectionOutline::Impl {
  Tempest::Attachment     mask;
  Tempest::RenderPipeline maskPipeline;
  Tempest::RenderPipeline compositePipeline;
  bool ready = false;

  void ensureResources(Tempest::Size size) {
    auto& device = Resources::device();
    if(mask.size()!=size) {
      if(!mask.isEmpty())
        Resources::recycle(std::move(mask));
      mask = device.attachment(Tempest::TextureFormat::RGBA8,size);
      }
    if(ready)
      return;

    Tempest::RenderState maskState;
    maskState.setCullFaceMode(Tempest::RenderState::CullMode::NoCull);
    maskState.setZTestMode(Tempest::RenderState::ZTestMode::LEqual);
    maskState.setZWriteEnabled(false);
    maskPipeline = device.pipeline(Tempest::Triangles,maskState,
                                   loadShader(device,"selection_outline.vert.sprv"),
                                   loadShader(device,"selection_outline.frag.sprv"));

    Tempest::RenderState compositeState;
    compositeState.setCullFaceMode(Tempest::RenderState::CullMode::NoCull);
    compositeState.setZTestMode(Tempest::RenderState::ZTestMode::Always);
    compositeState.setZWriteEnabled(false);
    compositeState.setBlendSource(Tempest::RenderState::BlendMode::SrcAlpha);
    compositeState.setBlendDest(Tempest::RenderState::BlendMode::OneMinusSrcAlpha);
    compositePipeline = device.pipeline(Tempest::Triangles,compositeState,
                                        loadShader(device,"triangle.vert.sprv"),
                                        loadShader(device,"selection_outline_composite.frag.sprv"));
    ready = true;
    }
  };

SelectionOutline::SelectionOutline()
  :impl(new Impl()) {
  }

SelectionOutline::~SelectionOutline() = default;

void SelectionOutline::render(Tempest::Encoder<Tempest::CommandBuffer>& cmd,
                              Tempest::Attachment& target, Tempest::ZBuffer& sceneDepth,
                              const Camera& camera, const WorldEdit& world) {
  if(!world.hasSelection() || target.isEmpty() || sceneDepth.isEmpty())
    return;

  std::vector<WorldEdit::SelectedMesh> meshes;
  world.selectedMeshes(meshes);
  if(meshes.empty())
    return;

  // The mask must use exactly the renderer depth-buffer resolution. Otherwise
  // depth samples and selected fragments do not address the same pixels and a
  // dilated mask turns the mismatch into a dense moire pattern.
  impl->ensureResources(sceneDepth.size());

  MaskPush maskPush;
  maskPush.viewProject = camera.viewProj();
  maskPush.params      = Tempest::Vec4(0.00001f,0.f,0.f,0.f);

  cmd.setDebugMarker("Selection outline mask");
  cmd.setFramebuffer({{impl->mask,Tempest::Vec4(0.f),Tempest::Preserve}},
                     {sceneDepth,Tempest::Readonly});
  cmd.setViewport(0,0,sceneDepth.w(),sceneDepth.h());
  cmd.setPipeline(impl->maskPipeline);
  for(const auto& draw:meshes) {
    if(draw.mesh==nullptr || draw.indexCount==0)
      continue;
    maskPush.model = draw.transform;
    cmd.setPushData(maskPush);
    cmd.draw(draw.mesh->vbo,draw.mesh->ibo,draw.indexOffset,draw.indexCount);
    }

  cmd.setDebugMarker("Selection outline composite");
  cmd.setFramebuffer({{target,Tempest::Preserve,Tempest::Preserve}});
  cmd.setViewport(0,0,target.w(),target.h());
  cmd.setBinding(0,impl->mask,Tempest::Sampler::nearest(Tempest::ClampMode::ClampToEdge));
  cmd.setPipeline(impl->compositePipeline);
  CompositePush compositePush;
  compositePush.color    = Tempest::Vec4(1.f,0.68f,0.05f,0.95f);
  compositePush.viewport = Tempest::Vec2(float(target.w()),float(target.h()));
  compositePush.radius   = 3.f;
  compositePush.padding  = 0.f;
  cmd.setPushData(compositePush);
  cmd.draw(nullptr,0,3);
  }
