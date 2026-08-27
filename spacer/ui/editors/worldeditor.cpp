#include "worldeditor.h"

#include <Tempest/Painter>
#include <Tempest/Log>

#include <string>

#include "ui/property/property.h"
#include "ui/property/propertylist.h"
#include "ui/objects/worldedit.h"

#include "editorwindow.h"
#include "resources.h"
#include "utils/dbgpainter.h"

using namespace Tempest;

WorldEditor::WorldEditor() {
  setFocusPolicy(Tempest::ClickFocus);

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
    camera.setPosition(Vec3(0,500,0));
    camera.setSpin(PointF(0));
    camera.setAngles(camera.spin());
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

void WorldEditor::processKeyboard(Tempest::KeyEvent& e) {
  const bool dw = e.type()==KeyEvent::KeyDown;
  if(e.key==KeyEvent::K_W)
    ctrl[KeyCodec::Forward] = dw;
  if(e.key==KeyEvent::K_A)
    ctrl[KeyCodec::Left] = dw;
  if(e.key==KeyEvent::K_S)
    ctrl[KeyCodec::Back] = dw;
  if(e.key==KeyEvent::K_D)
    ctrl[KeyCodec::Right] = dw;
  }

void WorldEditor::keyDownEvent(Tempest::KeyEvent& e) {
  if(e.key==KeyEvent::K_1) {
    gizmo.setMode(Im3dGizmo::Mode::Translation);
    e.accept();
    }
  else if(e.key==KeyEvent::K_2) {
    gizmo.setMode(Im3dGizmo::Mode::Rotation);
    e.accept();
    }
  else if(e.key==KeyEvent::K_3) {
    gizmo.setMode(Im3dGizmo::Mode::Scale);
    e.accept();
    }
  processKeyboard(e);
  update();
  }

void WorldEditor::keyUpEvent(Tempest::KeyEvent& e) {
  processKeyboard(e);
  update();
  }

void WorldEditor::mouseDownEvent(Tempest::MouseEvent& e) {
  mpos = e.pos();
  if(e.button==MouseEvent::ButtonLeft) {
    leftMouseDown = true;
    if(level!=nullptr && !gizmo.isHovered() && !gizmo.isActive()) {
      updateCursorRay();
      level->select(cursorRayOrigin,cursorRayDirection,camera.zFar());
      suppressGizmo = true;
      }
    }
  e.accept();
  update();
  }

void WorldEditor::mouseUpEvent(Tempest::MouseEvent& e) {
  mpos = e.pos();
  if(e.button==MouseEvent::ButtonLeft) {
    leftMouseDown = false;
    suppressGizmo = false;
    }
  e.accept();
  update();
  }

void WorldEditor::mouseMoveEvent(Tempest::MouseEvent& e) {
  mpos = e.pos();
  e.accept();
  update();
  }

void WorldEditor::mouseDragEvent(Tempest::MouseEvent& e) {
  const auto dp = (e.pos()-mpos);
  mpos = e.pos();

  if(e.button!=MouseEvent::ButtonRight) {
    e.accept();
    update();
    return;
    }

  PointF dpScaled = PointF(dp.x, dp.y);
  dpScaled.x/=float(w());
  dpScaled.y/=float(h());

  static float mul = 270.f;
  dpScaled *= mul;

  auto rot = camera.spin() + PointF(dpScaled.y,-dpScaled.x);
  camera.setSpin(rot);
  camera.setAngles(camera.spin());
  e.accept();
  update();
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

  paintEditorOverlay(p);
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

  tickCamera(16); //TODO
  if(level!=nullptr)
    renderer.draw(sceneImage, cmd, cmdId, level->view(), camera);
  gizmo.render(cmd,sceneImage,cmdId);
  }

void WorldEditor::updateCursorRay() {
  if(w()<=0 || h()<=0)
    return;

  auto invViewProj = camera.viewProj();
  invViewProj.inverse();

  const float x = float(mpos.x)*2.f/float(w())-1.f;
  const float y = float(mpos.y)*2.f/float(h())-1.f;
  Tempest::Vec3 farPoint = {x,y,1.f};
  invViewProj.project(farPoint);

  cursorRayOrigin = camera.originLwc();
  const auto direction = farPoint-cursorRayOrigin;
  if(direction.length()>1e-7f)
    cursorRayDirection = Tempest::Vec3::normalize(direction);
  }

void WorldEditor::paintEditorOverlay(Tempest::Painter& painter) {
  if(level==nullptr || w()<=0 || h()<=0)
    return;

  updateCursorRay();
  bool changed = false;
  Tempest::Matrix4x4 transform;
  if(level->hasSelection()) {
    transform = level->selectedTransform();
    changed = gizmo.prepare(camera,cursorRayOrigin,cursorRayDirection,
                            leftMouseDown && !suppressGizmo,w(),h(),&transform);
    if(changed) {
      level->setSelectedTransform(transform);
      update();
      }
    }
  else {
    gizmo.prepare(camera,cursorRayOrigin,cursorRayDirection,
                  leftMouseDown && !suppressGizmo,w(),h(),nullptr);
    }

  DbgPainter text(painter,camera.viewProj(),w(),h());
  const char* mode = gizmo.mode()==Im3dGizmo::Mode::Translation ? "[1] Move   2 Rotate   3 Scale" :
                     gizmo.mode()==Im3dGizmo::Mode::Rotation    ? "1 Move   [2] Rotate   3 Scale" :
                                                                 "1 Move   2 Rotate   [3] Scale";
  text.drawText(8,8,mode);
  if(level->hasSelection()) {
    std::string label = "Selected: ";
    label += level->selectedName();
    text.drawText(8,28,label);
    }
  }

void WorldEditor::tickCamera(uint64_t dt) {
  if(ctrl[KeyCodec::Forward]) {
    camera.moveForward(dt);
    update();
    }
  if(ctrl[KeyCodec::Left]) {
    camera.moveLeft(dt);
    update();
    }
  if(ctrl[KeyCodec::Back]) {
    camera.moveBack(dt);
    update();
    }
  if(ctrl[KeyCodec::Right]) {
    camera.moveRight(dt);
    update();
    }
  camera.tick(dt);
  }
