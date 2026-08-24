#include "worldeditor.h"

#include <Tempest/Painter>

#include "ui/property/property.h"
#include "ui/property/propertylist.h"

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
  }

WorldEditor::~WorldEditor() {
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

void WorldEditor::paintEvent(Tempest::PaintEvent& e) {
  Painter p(e);
  p.setBrush(Color(0,0,0.4,1));
  p.drawRect(0, 0, w(), h());
  }
