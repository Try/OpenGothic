#include "worldeditor.h"

#include <Tempest/Painter>
#include <Tempest/Log>
#include <Tempest/ListView>

#include "ui/property/propertydelegate.h"
#include "ui/projectitemview.h"
#include "ui/vobtreedelegate.h"
#include "objects/worldedit.h"
#include "objects/rayquery.h"
#include "editorwindow.h"
#include "resources.h"
#include "utils/fileext.h"

using namespace Tempest;

struct WorldEditor::Gizmo {
  static constexpr float TMax = 1e30f;

  Gizmo(Vec3 origin):origin(origin) {}

  Vec3 origin;

  struct HitResult {
    float hitT = TMax;
    int   axis = -1;
    };

  static void handleHit(HitResult& hit, float t, int axis) {
    if(t >= hit.hitT)
      return;
    // hit.norm = n;
    hit.hitT = t;
    hit.axis = axis;
    }

  static float sign(float x) {
    if(x>0.f)
      return +1.f;
    if(x<0.f)
      return -1.f;
    return 0.f;
    }

  static float dot(Vec2 a, Vec2 b) { return Vec2::dotProduct(a,b); }

  static float dot(Vec3 a, Vec3 b) { return Vec3::dotProduct(a,b); }

  static float dot2(Vec3 d) { return Vec3::dotProduct(d,d); }

  static bool cylIntersect(HitResult& hit, int axis, Vec3 ro, Vec3 rd, Vec3 a, Vec3 b, float ra) {
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
      return false;//no intersection
    h = std::sqrt(h);
    float t = (-k1-h)/k2;
    // body
    float y = baoc + t*bard;
    if( y>0.0 && y<baba ) {
      handleHit(hit, t, axis);
      return true;
      }

    // caps
    t = ( ((y<0.0) ? 0.0 : baba) - baoc)/bard;
    if(abs(k1+k2*t) < h) {
      handleHit(hit, t, axis);
      return true;
      }

    return false; //no intersection
    }

  static bool sphereIntersect(HitResult& hit, int axis, Vec3 ro, Vec3 rd, Vec3 center, float radius) {
    Vec3 oc = ro - center;

    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;

    float h = b*b - c;

    if(h < 0.0)
      return false;

    h = sqrt(h);

    float t = -b - h;

    // Camera is inside sphere: use far intersection.
    if(t < 0.0)
      t = -b + h;

    if(t < 0.0)
      return false;

    handleHit(hit, t, axis);
    return true;
    }

  static bool coneIntersect(HitResult& hit, int axis, Vec3 ro, Vec3 rd, Vec3 pa, Vec3 pb, float ra, float rb) {
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
      if(dot2(oa*m2-rd*m1) < (ra*ra*m2*m2)) {
        // delayed division
        handleHit(hit, -m1/m2, axis);
        return true;
        }
      }
    else if(m9 > 0.0) {
      float t = -m9/m2;                     // NOTE delayed division
      if(dot2(ob+rd*t) < (rb*rb)) {
        handleHit(hit, t, axis);
        return true;
        }
      }

    // body
    float rr = ra - rb;
    float hy = m0 + rr*rr;
    float k2 = m0*m0    - m2*m2*hy;
    float k1 = m0*m0*m3 - m1*m2*hy + m0*ra*(rr*m2*1.0        );
    float k0 = m0*m0*m5 - m1*m1*hy + m0*ra*(rr*m1*2.0 - m0*ra);
    float h  = k1*k1 - k2*k0;
    if(h < 0.0)
      return false; //no intersection
    float t = (-k1-sqrt(h))/k2;
    float y = m1 + t*m2;
    if(y < 0.0 || y > m0)
      return false; //no intersection
    handleHit(hit, t, axis);
    return true;
    }

  static void arrowIntersect(HitResult& hit, int axis, Vec3 ro, Vec3 rd, Vec3 origin, float scale) {
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

    cylIntersect (hit, axis, ro, rd, origin, origin + off1, 4.0 * scale);
    coneIntersect(hit, axis, ro, rd, origin + off1, origin + off2, 10.0 * scale, 0);
    }

  bool rotationRingIntersect(HitResult& hit, int axis, Vec3 ro, Vec3 rd, float rIn, float rOut) {
    ro -= origin;
    if(axis == 0) {
      ro = Vec3(ro.z, ro.y, ro.x);
      rd = Vec3(rd.z, rd.y, rd.x);
      }
    else if(axis == 1) {
      ro = Vec3(ro.x, ro.z, ro.y);
      rd = Vec3(rd.x, rd.z, rd.y);
      }
    else {
      ro = ro;
      rd = rd;
      }

    float po  = 1.0;
    float Ra2 = 0.5*(rIn + rOut); Ra2*=Ra2;
    float ra2 = 0.5*(rIn - rOut); ra2*=ra2;

    float m = Vec3::dotProduct(ro, ro);
    float n = Vec3::dotProduct(ro, rd);
    float k = (m + Ra2 - ra2)/2.0;
    float k3 = n;
    float k2 = n*n - Ra2*dot(Vec2(rd.x,rd.y),Vec2(rd.x,rd.y)) + k;
    float k1 = n*k - Ra2*dot(Vec2(rd.x,rd.y),Vec2(ro.x,ro.y));
    float k0 = k*k - Ra2*dot(Vec2(ro.x,ro.y),Vec2(ro.x,ro.y));

    if(abs(k3*(k3*k3-k2)+k1) < 0.01) {
      po = -1.0;
      float tmp=k1; k1=k3; k3=tmp;
      k0 = 1.0/k0;
      k1 = k1*k0;
      k2 = k2*k0;
      k3 = k3*k0;
      }

    float c2 = k2*2.0 - 3.0*k3*k3;
    float c1 = k3*(k3*k3-k2)+k1;
    float c0 = k3*(k3*(c2+2.0*k2)-8.0*k1)+4.0*k0;
    c2 /= 3.0;
    c1 *= 2.0;
    c0 /= 3.0;
    float Q = c2*c2 + c0;
    float R = c2*c2*c2 - 3.0*c2*c0 + c1*c1;
    float h = R*R - Q*Q*Q;

    if( h>=0.0 ) {
      h = sqrt(h);
      float v = sign(R+h)*pow(abs(R+h),1.0/3.0); // cube root
      float u = sign(R-h)*pow(abs(R-h),1.0/3.0); // cube root
      Vec2 s = Vec2( (v+u)+4.0*c2, (v-u)*sqrt(3.0));
      float y = sqrt(0.5*(s.length()+s.x));
      float x = 0.5*s.y/y;
      float r = 2.0*c1/(x*x+y*y);
      float t1 =  x - r - k3; t1 = (po<0.0)?2.0/t1:t1;
      float t2 = -x - r - k3; t2 = (po<0.0)?2.0/t2:t2;
      float t = TMax;
      if( t1>0.0 ) t=t1;
      if( t2>0.0 ) t=std::min(t,t2);

      handleHit(hit, t, axis);
      return true;
      }

    float sQ = sqrt(Q);
    float w = sQ*cos( acos(-R/(sQ*Q)) / 3.0 );
    float d2 = -(w+c2);
    if( d2<0.0 )
      return false;
    float d1 = sqrt(d2);
    float h1 = sqrt(w - 2.0*c2 + c1/d1);
    float h2 = sqrt(w - 2.0*c2 - c1/d1);
    float t1 = -d1 - h1 - k3; t1 = (po<0.0)?2.0/t1:t1;
    float t2 = -d1 + h1 - k3; t2 = (po<0.0)?2.0/t2:t2;
    float t3 =  d1 - h2 - k3; t3 = (po<0.0)?2.0/t3:t3;
    float t4 =  d1 + h2 - k3; t4 = (po<0.0)?2.0/t4:t4;
    float t = TMax;
    if( t1>0.0 ) t=t1;
    if( t2>0.0 ) t=std::min(t,t2);
    if( t3>0.0 ) t=std::min(t,t3);
    if( t4>0.0 ) t=std::min(t,t4);

    handleHit(hit, t, axis);
    return true;
    }

  static int intersectPos(const Matrix4x4& v, const Matrix4x4& vp, Vec2 pos, Vec3 origin) {
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

    HitResult ret = {};
    ret.hitT = TMax;

    Gizmo giz{origin};
    for(int axis = 0; axis<3; ++axis) {
      giz.arrowIntersect(ret, axis, src, dir, origin, scale);
      }
    return ret.axis;
    }

  static int intersectRot(const Matrix4x4& v, const Matrix4x4& vp, Vec2 pos, Vec3 origin) {
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

    HitResult ret = {};
    ret.hitT = TMax;

    Gizmo giz{origin};
    for(int axis = 0; axis<3; ++axis) {
      giz.rotationRingIntersect(ret, axis, src, dir, 145.0 * scale, 155.0 * scale);
      }
    giz.sphereIntersect(ret, -1, src, dir, origin, 145.0 * scale);
    return ret.axis;
    }
  };


WorldEditor::WorldEditor() {
  setFocusPolicy(Tempest::ClickFocus);

  onDelete = Shortcut(*this, Event::M_NoModifier, Event::K_Delete);
  onDelete.onActivated.bind(this, &WorldEditor::deleteVob);

  timer.timeout.bind(this, &WorldEditor::tick);
  timer.start(16);
  renderer.setLightsHud(&Assets::inst().im.pointLight);
  EditorWindow::onUpdate3D.bind(this, &WorldEditor::update3d);
  }

WorldEditor::~WorldEditor() {
  EditorWindow::onUpdate3D.ubind(this, &WorldEditor::update3d);
  }

std::string_view WorldEditor::title() const {
  return "";
  }

void WorldEditor::preload(ProjectItem&) const {
  }

bool WorldEditor::load(ProjectItem& it) {
  try {
    level = it.get();

    camera.setMarvinMode(Camera::M_Free);
    camera.setPosition(Vec3(0,500,0));
    camera.setSpin(PointF(0));
    camera.setAngles(camera.spin());

    treeDelegate->setWorld(*level);

    return true;
    }
  catch(...) {
    Tempest::Log::e("unable to load landscape mesh");
    return false;
    }
  }

BaseEditor::BaseTool* WorldEditor::createToolpanel(ToolWindow::Tool tool) {
  if(tool==ToolWindow::T_VobTree) {
    auto ctrl = new BaseTool();
    auto& list     = ctrl->addWidget(new Tempest::ListView());
    auto& delegate = *list.setDelegate(new VobTreeDelegate());
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
    delegate.onChanged = [this](std::unique_ptr<Command::Action<WorldEdit>>& cmd, bool commit) {
      setVobProperty(cmd, commit);
      };

    propertyDelegate = &delegate;
    return ctrl;
    }
  return nullptr;
  }

void WorldEditor::undo() {
  timeline.undo(*level);
  propertyDelegate->update();
  treeDelegate->update();
  update();
  }

void WorldEditor::redo() {
  timeline.redo(*level);
  propertyDelegate->update();
  treeDelegate->update();
  //selVob = level->root();
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
  if(e.key==KeyEvent::K_1) {
    gizmoMode = GizmoMode::Drag;
    }
  else if(e.key==KeyEvent::K_2) {
    gizmoMode = GizmoMode::Rotate;
    }
  else if(e.key==KeyEvent::K_3) {
    gizmoMode = GizmoMode::Scale;
    }
  update();
  }

void WorldEditor::mouseDownEvent(Tempest::MouseEvent& e) {
  mpos  = e.pos();
  state = State::T_Idle;

  if(e.button==Tempest::Event::ButtonLeft) {
    const int giz = gizmoQuery(mpos);
    if(0<=giz && giz<3 && gizmoMode==GizmoMode::Drag) {
      state = State(uint32_t(State::T_DragX) + giz);
      dragVob(mpos, *selVob, state, true);
      }
    else if(0<=giz && giz<3 && gizmoMode==GizmoMode::Rotate) {
      state = State(uint32_t(State::T_RotX) + giz);
      rotateVob(mpos, *selVob, state, true);
      }
    else {
      if(auto vob = rayQuery(mpos).vob())
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
  else if(state==State::T_RotX || state==State::T_RotY || state==State::T_RotZ) {
    if(selVob!=nullptr && selVob->get()!=nullptr) {
      rotateVob(mpos, *selVob, state);
      }
    }
  }

void WorldEditor::moveDropOver(DropOverEvent& ev) {
  if(auto itm = dynamic_cast<ProjectItemView*>(&ev.drop())) {
    if(itm->it.type()==ProjectItem::T_StaticMesh) {
      auto name = std::string(itm->it.name());
      FileExt::exchangeExt(name,"MRM","3DS");

      const auto query = rayQuery(ev.pos());

      insertVob.reset(new WorldEdit::Vob());
      insertVob->setVisual(*level, name);
      insertVob->setCollision(*level, false);
      insertVob->setPosition(query.hitPos());

      ev.accept();
      ev.setUiVisible(false);
      update();
      }
    if(itm->it.type()==ProjectItem::T_Texture) {
      // decals?
      }
    }
  }

void WorldEditor::dropDone(DropOverEvent& ev) {
  const auto query = rayQuery(ev.pos());

  insertVob->setCollision(*level, true);
  insertVob->setPosition(query.hitPos());

  auto vob = insertVob.get();
  timeline.push(*level, new CmdNewVob(insertVob.release()));
  invalidateTab();
  selectVob(vob);
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
    if(gizmoMode == GizmoMode::Drag) {
      const int axi = Gizmo::intersectPos(camera.view(), camera.viewProj(),
                                          pos, Vec3(origin.x, origin.y, origin.z));
      return axi;
      }
    else if(gizmoMode == GizmoMode::Rotate) {
      const int axi = Gizmo::intersectRot(camera.view(), camera.viewProj(),
                                          pos, Vec3(origin.x, origin.y, origin.z));
      return axi;
      }
    }
  return -1;
  }

auto WorldEditor::rayQuery(Tempest::Point mpos) -> RayQuery {
  RayQuery query(camera.view(), camera.viewProj(), mpos, size());
  query.proceed(*level);
  return query;
  }

void WorldEditor::dragVob(Tempest::Point mpos, WorldEdit::Vob& vob, State st, bool init) {
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
  auto orig = selVob->position();

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

  Vec3 hit  = src + t*dir;
  Vec3 vpos = Vec3(orig.x, orig.y, orig.z);
  if(state==State::T_DragX) {
    vpos.x = hit.x;
    }
  else if(state==State::T_DragY) {
    vpos.y = hit.y;
    }
  else if(state==State::T_DragZ) {
    vpos.z = hit.z;
    }

  if(init) {
    gizmoState.pos0 = vpos - orig;
    return;
    }
  vpos = (vpos - gizmoState.pos0);
  timeline.push(*level, new CmdMoveVob(&vob, vpos), false);
  invalidateTab();
  update();
  }

void WorldEditor::rotateVob(Tempest::Point mpos, WorldEdit::Vob& vob, State st, bool init) {
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
  auto orig = selVob->position();
  auto ang  = Vec3();

  if(state==State::T_RotX) {
    float t   = (orig.x - src.x)/dir.x;
    Vec3  hit = src + t*dir;
    Vec3  dt  = hit - orig;
    ang.x = float(std::atan2(dt.y, dt.z));
    }
  else if(state==State::T_RotY) {
    float t   = (orig.y - src.y)/dir.y;
    Vec3  hit = src + t*dir;
    Vec3  dt  = hit - orig;
    ang.y = -float(std::atan2(dt.x, dt.z));
    }
  else if(state==State::T_RotZ) {
    float t   = (orig.z - src.z)/dir.z;
    Vec3  hit = src + t*dir;
    Vec3  dt  = hit - orig;
    ang.z = float(std::atan2(dt.x, dt.y));
    }

  if(init) {
    auto rot = vob.rotation();
    gizmoState.rot0 = Matrix4x4(rot[0][0], rot[0][1], rot[0][2], 0,
                                rot[1][0], rot[1][1], rot[1][2], 0,
                                rot[2][0], rot[2][1], rot[2][2], 0,
                                0, 0, 0, 1);
    gizmoState.ang0 = ang;
    return;
    }

  auto angle = ang - gizmoState.ang0;
  auto mat   = Tempest::Matrix4x4::mkIdentity();
  if(state==State::T_RotX) {
    mat.rotateOX(angle.x*float(180.0/M_PI));
    }
  else if(state==State::T_RotY) {
    mat.rotateOY(angle.y*float(180.0/M_PI));
    }
  else if(state==State::T_RotZ) {
    mat.rotateOZ(angle.z*float(180.0/M_PI));
    }

  auto r = gizmoState.rot0;
  r.mul(mat);

  auto rt = zenkit::Mat3(r[0][0], r[0][1], r[0][2],
                         r[1][0], r[1][1], r[1][2],
                         r[2][0], r[2][1], r[2][2]);
  timeline.push(*level, new CmdRotateVob(&vob, rt.transpose()), false);
  invalidateTab();
  update();
  }

void WorldEditor::deleteVob() {
  if(selVob!=nullptr) {
    timeline.push(*level, new CmdDeleteVob(selVob));
    invalidateTab();
    treeDelegate->update();
    selectVob(nullptr);
    update();
    }
  }

void WorldEditor::selectVob(WorldEdit::Vob* vob) {
  selVob = vob;
  treeDelegate->setVob(selVob);
  propertyDelegate->setVob(selVob);
  update();
  }

void WorldEditor::setVobProperty(std::unique_ptr<Command::Action<WorldEdit>>& cmd, bool commit) {
  timeline.push(*level, cmd.release(), commit);
  invalidateTab();
  update();
  }

void WorldEditor::updateGizmo() {
  if(selVob==nullptr || selVob->get()==nullptr) {
    renderer.setGizmo(false, Vec3(), 0);
    return;
    }
  const auto pos = selVob->get()->position;
  renderer.setGizmo(true, Vec3(pos.x,pos.y,pos.z), int(gizmoMode));
  }
