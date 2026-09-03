#include "worldeditor.h"

#include <Tempest/Painter>
#include <Tempest/Log>
#include <Tempest/ListView>

#include "ui/property/propertydelegate.h"
#include "ui/vobtreedelegate.h"
#include "objects/worldedit.h"
#include "editorwindow.h"
#include "resources.h"

using namespace Tempest;

WorldEditor::WorldEditor() {
  setFocusPolicy(Tempest::ClickFocus);

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

  timer.timeout.bind(this, &WorldEditor::tick);
  timer.start(16);
  renderer.setLightsHud(&Assets::inst().im.pointLight);
  EditorWindow::onUpdate3D.bind(this, &WorldEditor::update3d);
  }

WorldEditor::~WorldEditor() {
  EditorWindow::onUpdate3D.ubind(this, &WorldEditor::update3d);
  }

std::string_view WorldEditor::title() const {
  return "World editor";
  }

BaseEditor::BaseTool* WorldEditor::createToolpanel(ToolWindow::Tool tool) {
  if(tool==ToolWindow::T_VobTree) {
    auto ctrl = new BaseTool();
    auto& list     = ctrl->addWidget(new Tempest::ListView());
    auto& delegate = *list.setDelegate(new VobTreeDelegate(*level));
    ctrl->setLayout(Vertical);
    delegate.onVobSelected.bind(this, &WorldEditor::selectVob);

    treeDelegate = &delegate;
    return ctrl;
    }
  if(tool==ToolWindow::T_VobProp) {
    auto ctrl = new BaseTool();
    auto& list     = ctrl->addWidget(new Tempest::ListView());
    auto& delegate = *list.setDelegate(new PropertyDelegate());
    ctrl->setLayout(Vertical);

    propertyDelegate = &delegate;
    return ctrl;
    }
  return nullptr;
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
  processKeyboard(e);
  update();
  }

void WorldEditor::keyUpEvent(Tempest::KeyEvent& e) {
  processKeyboard(e);
  update();
  }

void WorldEditor::mouseDownEvent(Tempest::MouseEvent& e) {
  mpos = e.pos();
  if(auto vob = rayQuery(mpos))
    selectVob(*vob);
  update();
  }

void WorldEditor::mouseDragEvent(Tempest::MouseEvent& e) {
  const auto dp = (e.pos()-mpos);
  mpos = e.pos();

  PointF dpScaled = PointF(dp.x, dp.y);
  dpScaled.x/=float(w());
  dpScaled.y/=float(h());

  static float mul = 270.f;
  dpScaled *= mul;

  auto rot = camera.spin() + PointF(dpScaled.y,-dpScaled.x);
  camera.setSpin(rot);
  camera.setAngles(camera.spin());
  update();
  }

void WorldEditor::moveDropOver(DropOverEvent& ev) {
  }

void WorldEditor::dropDone(DropOverEvent& ev) {
  }

void WorldEditor::paintEvent(PaintEvent& e) {
  Painter p(e);
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

  renderer.draw(sceneImage, cmd, cmdId, level->view(), camera);
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

void WorldEditor::tick() {
  tickCamera(16);
  }

const WorldEdit::Vob* WorldEditor::rayQuery(Tempest::Point mpos) {
  return level->rayQuery(camera.view(), camera.viewProj(), mpos, size());
  }

void WorldEditor::selectVob(const WorldEdit::Vob& vob) {
  if(vob.get()==nullptr)
    return;
  const auto pos = vob.get()->position;
  renderer.setGizmo(true, Vec3(pos.x,pos.y,pos.z));
  treeDelegate->setVob(&vob);
  propertyDelegate->setVob(&vob);
  update();
  }
