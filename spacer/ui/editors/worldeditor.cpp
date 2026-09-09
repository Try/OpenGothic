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

struct WorldEditor::Gizmo {
  static float dot2(Vec3 d) { return Vec3::dotProduct(d,d); }

  static float cylIntersect(Vec3 ro, Vec3 rd, Vec3 a, Vec3 b, float ra ) {
    Vec3  ba = b  - a;
    Vec3  oc = ro - a;

    float baba = Vec3::dotProduct(ba,ba);
    float bard = Vec3::dotProduct(ba,rd);
    float baoc = Vec3::dotProduct(ba,oc);
    float k2   = baba            - bard*bard;
    float k1   = baba*Vec3::dotProduct(oc,rd) - baoc*bard;
    float k0   = baba*Vec3::dotProduct(oc,oc) - baoc*baoc - ra*ra*baba;
    float h    = k1*k1 - k2*k0;

    if( h<0.0 )
      return -1.0;//no intersection
    h = std::sqrt(h);
    float t = (-k1-h)/k2;
    // body
    float y = baoc + t*bard;
    if( y>0.0 && y<baba )
      return t;

    // caps
    t = ( ((y<0.0) ? 0.0 : baba) - baoc)/bard;
    if(abs(k1+k2*t) < h)
      return t;

    return -1.0; //no intersection
    }

  static float coneIntersect(Vec3 ro, Vec3 rd, Vec3 pa, Vec3 pb, float ra, float rb) {
    Vec3  ba = pb - pa;
    Vec3  oa = ro - pa;
    Vec3  ob = ro - pb;
    float m0 = Vec3::dotProduct(ba,ba);
    float m1 = Vec3::dotProduct(oa,ba);
    float m2 = Vec3::dotProduct(rd,ba);
    float m3 = Vec3::dotProduct(rd,oa);
    float m5 = Vec3::dotProduct(oa,oa);
    float m9 = Vec3::dotProduct(ob,ba);

    // caps
    if(m1 < 0.0) {
      if(dot2(oa*m2-rd*m1) < (ra*ra*m2*m2)) // delayed division
        return (-m1/m2);
      }
    else if(m9 > 0.0) {
      float t = -m9/m2;                     // NOTE delayed division
      if(dot2(ob+rd*t) < (rb*rb))
        return t;
      }

    // body
    float rr = ra - rb;
    float hy = m0 + rr*rr;
    float k2 = m0*m0    - m2*m2*hy;
    float k1 = m0*m0*m3 - m1*m2*hy + m0*ra*(rr*m2*1.0        );
    float k0 = m0*m0*m5 - m1*m1*hy + m0*ra*(rr*m1*2.0 - m0*ra);
    float h  = k1*k1 - k2*k0;
    if(h < 0.0)
      return -1.0; //no intersection
    float t = (-k1-sqrt(h))/k2;
    float y = m1 + t*m2;
    if(y < 0.0 || y > m0)
      return -1.0; //no intersection
    return t;
    }

  static float arrowIntersect(Vec3 ro, Vec3 rd, Vec3 origin, int axis, float scale) {
    //int axis = 0;

    Vec3 off0 = Vec3(0), off1 = Vec3(0), off2 = Vec3(0);
    if(axis==0) {
      off0.x = scale*10;
      off1.x = scale*160;
      off2.x = scale*200;
      }
    else if(axis==1) {
      off0.y = scale*10;
      off1.y = scale*160;
      off2.y = scale*200;
      }
    else if(axis==2) {
      off0.z = scale*10;
      off1.z = scale*160;
      off2.z = scale*200;
      }

    float body = cylIntersect (ro, rd, origin, origin + off1, 4.0 * scale);
    float cap  = coneIntersect(ro, rd, origin + off1, origin + off2, 10.0 * scale, 0);
    if(body>=0 && (body<cap || cap<=0))
      return body;
    return cap;
    }

  static int intersect(const Matrix4x4& v, const Matrix4x4& vp, Vec2 pos, Vec3 origin) {
    auto vInv  = v;
    auto vpInv = vp;
    vInv.inverse();
    vpInv.inverse();

    Vec3 dst = {pos.x, pos.y, 1};
    vpInv.project(dst);

    Vec3 src = {pos.x, pos.y, 0};
    vInv.project(src);

    const Vec4  pos4  = vp * Vec4(origin.x, origin.y, origin.z, 1.0);
    const float scale = pos4.w/1000.0;
    const auto  dir   = Vec3::normalize(dst-src);

    int   ret  = -1;
    float tMin = std::numeric_limits<float>::max();
    for(int axis = 0; axis<3; ++axis) {
      const float v = arrowIntersect(src, dir, origin, axis, scale);
      if(v > tMin || v<0)
        continue;
      tMin = v;
      ret  = axis;
      }
    return ret;
    }
  };


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
  timeline.undo(*level);
  update();
  }

void WorldEditor::redo() {
  timeline.redo(*level);
  update();
  }

bool WorldEditor::hasUnsavedChanges() const {
  return timeline.hasUnsavedChanges();
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
  if(e.key==Tempest::Event::K_Delete && selVob!=nullptr) {
    timeline.push(*level, new CmdDeleteVob(selVob));
    treeDelegate->invalidate();
    selectVob(nullptr);
    }
  update();
  }

void WorldEditor::mouseDownEvent(Tempest::MouseEvent& e) {
  mpos  = e.pos();
  state = State::T_Idle;

  if(e.button==Tempest::Event::ButtonLeft) {
    const int giz = gizmoQuery(mpos);
    if(0<=giz && giz<3) {
      state = State(uint32_t(State::T_DragX) + giz);
      }
    else {
      if(auto vob = rayQuery(mpos))
        selectVob(vob);
      }
    }
  else if(e.button==Tempest::Event::ButtonRight) {
    state = State::T_WASD;
    }

  update();
  }

void WorldEditor::mouseUpEvent(Tempest::MouseEvent& e) {
  state = State::T_Idle;
  update();
  }

void WorldEditor::mouseDragEvent(Tempest::MouseEvent& e) {
  const auto dp = (e.pos()-mpos);
  mpos = e.pos();

  if(state==State::T_WASD) {
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
  else if(state==State::T_DragX || state==State::T_DragY || state==State::T_DragZ) {
    if(selVob!=nullptr && selVob->get()!=nullptr) {
      dragVob(mpos, *selVob, state);
      }
    }
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

  updateGizmo();
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
  if(state==State::T_WASD) {
    tickCamera(16);
    }
  }

int WorldEditor::gizmoQuery(Tempest::Point mpos) const {
  if(selVob!=nullptr && selVob->get()!=nullptr) {
    Tempest::Vec2 pos = {mpos.x/float(w()), mpos.y/float(h())};
    pos = 2.f*pos - 1.f;

    const auto origin = selVob->get()->position;
    const int  axi    = Gizmo::intersect(camera.view(), camera.viewProj(),
                                         pos, Vec3(origin.x, origin.y, origin.z));
    return axi;
    }
  return -1;
  }

WorldEdit::Vob* WorldEditor::rayQuery(Tempest::Point mpos) {
  return level->rayQuery(camera.view(), camera.viewProj(), mpos, size());
  }

void WorldEditor::dragVob(Tempest::Point mpos, const WorldEdit::Vob& vob, State st) {
  Tempest::Vec2 pos = {mpos.x/float(w()), mpos.y/float(h())};
  pos = 2.f*pos - 1.f;

  auto vInv  = camera.view();
  auto vpInv = camera.viewProj();
  vInv.inverse();
  vpInv.inverse();

  Vec3 dst = {pos.x, pos.y, 1};
  vpInv.project(dst);

  Vec3 src = {pos.x, pos.y, 0};
  vInv.project(src);

  Vec3 dir  = Vec3::normalize(dst-src);
  Vec3 adir = Vec3(std::abs(dir.x), std::abs(dir.y), std::abs(dir.z));
  auto orig = selVob->get()->position;

  if(state==State::T_DragX)
    adir.x = -1;
  else if(state==State::T_DragY)
    adir.y = -1;
  else if(state==State::T_DragZ)
    adir.z = -1;

  float t = 0;
  if(adir.x>=adir.y && adir.x>=adir.z)
    t = (orig.x - src.x)/dir.x;
  else if(adir.y>=adir.x && adir.y>=adir.z)
    t = (orig.y - src.y)/dir.y;
  else if(adir.z>=adir.x && adir.z>=adir.y)
    t = (orig.z - src.z)/dir.z;

  Vec3  hit = src + t*dir;

  Vec3  vpos = Vec3(orig.x, orig.y, orig.z);
  if(state==State::T_DragX) {
    vpos.x = hit.x;
    }
  else if(state==State::T_DragY) {
    vpos.y = hit.y;
    }
  else if(state==State::T_DragZ) {
    vpos.z = hit.z;
    }
  setVobPosition(selVob, level->root(), vpos);
  }

void WorldEditor::selectVob(WorldEdit::Vob* vob) {
  selVob = vob;
  treeDelegate->setVob(selVob);
  propertyDelegate->setVob(selVob);
  update();
  }

bool WorldEditor::setVobPosition(const WorldEdit::Vob* selVob, WorldEdit::Vob& vob, Tempest::Vec3 pos) {
  if(&vob==selVob) {
    timeline.push(*level, new CmdMoveVob(&vob, pos), false);
    update();
    return true;
    }
  for(size_t i=0; i<vob.size(); ++i)
    if(setVobPosition(selVob, vob[i], pos))
      return true;
  return false;
  }

void WorldEditor::updateGizmo() {
  if(selVob==nullptr || selVob->get()==nullptr) {
    renderer.setGizmo(false, Vec3());
    return;
    }
  const auto pos = selVob->get()->position;
  renderer.setGizmo(true, Vec3(pos.x,pos.y,pos.z));
  }
