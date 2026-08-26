#include "worldeditor.h"

#include <Tempest/Painter>
#include <Tempest/Log>

#include "ui/property/property.h"
#include "ui/property/propertylist.h"
#include "ui/objects/worldedit.h"

#include "editorwindow.h"
#include "resources.h"

using namespace Tempest;

WorldEditor::WorldEditor() {
  props = {
    {"Vob"},
    {"vobName",               Property::Type::Int1},
    {"visual",                Property::Type::Int1},
    {"showVisual",            Property::Type::Bool1},
    {"visualCamAlign",        Property::Type::Enum},
    {"visualAniMode",         Property::Type::Enum},
    {"visualAniModeStrength", Property::Type::Vec1},
    {"vobFarClipZScale",      Property::Type::Vec1},
    {"cdStatic",              Property::Type::Bool1},
    {"cdDyn",                 Property::Type::Bool1},
    {"staticVob",             Property::Type::Bool1},
    {"dynShadow",             Property::Type::Enum},
    {"zbias",                 Property::Type::Vec1},
    {"isAmbient",             Property::Type::Bool1},
    };

  try {
    // level.reset(new WorldEdit("dragonisland.zen"));
    level.reset(new WorldEdit("oldworld.zen"));
    camera.setMarvinMode(Camera::M_Free);
    camera.reset();
    camera.setPosition(Vec3(0,500,0));
    }
  catch(...) {
    Tempest::Log::e("unable to load landscape mesh");
    }

  EditorWindow::onUpdate3D.bind(this, &WorldEditor::update3d);
  }

WorldEditor::~WorldEditor() {
  EditorWindow::onUpdate3D.ubind(this, &WorldEditor::update3d);
  }

std::string_view WorldEditor::title() const {
  return "World editor";
  }

BaseEditor::BaseTool* WorldEditor::createToolpanel(ToolWindow::Tool tool) {
  if(tool!=ToolWindow::T_VobProp)
    return nullptr;
  auto ctrl = new BaseTool();
  auto& prop = ctrl->addWidget(new PropertyList(props));
  ctrl->setLayout(Vertical);
  // prop.onChanged.bind(this,&LevelEditor::onProperty);
  return ctrl;
  }

void WorldEditor::undo() {
  }

void WorldEditor::redo() {
  }

void WorldEditor::moveDropOver(DropOverEvent& ev) {
  }

void WorldEditor::dropDone(DropOverEvent& ev) {
  }

void WorldEditor::paintEvent(PaintEvent& e) {
  Painter p(e);
  p.setBrush(Color(0,0,0.4,1));
  p.drawRect(0, 0, w(), h());

  p.setBrush(textureCast<Texture2d&>(sceneImage));
  p.drawRect(0, 0, w(), h(),
             0, 0, sceneImage.w(), sceneImage.h());
  }

void WorldEditor::resizeEvent(SizeEvent& e) {
  camera.setViewport(uint32_t(w()),uint32_t(h()));
  }

void WorldEditor::update3d(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t cmdId) {
  if(size().isEmpty())
    return;

  auto& device = Resources::device();
  if(sceneImage.size()!=size()) {
    Resources::recycle(std::move(sceneImage));
    sceneImage = device.attachment(TextureFormat::RGBA8, size());
    update();
    }

  if(!hasFocus() && !needToUpdate())
    return;

  camera.tick(16); //TODO
  renderer.draw(sceneImage, cmd, cmdId, level->view(), camera);
  }
