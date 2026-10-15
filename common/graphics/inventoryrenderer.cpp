#include "inventoryrenderer.h"

#include "world/objects/item.h"
#include "graphics/mesh/protomesh.h"

#include "shaders.h"

using namespace Tempest;

InventoryRenderer::InventoryRenderer()
  :visual(scene,std::pair<Vec3,Vec3>()),itmGroup(visual) {
  Tempest::Matrix4x4 p, mv, shMv[Resources::ShadowLayers];
  p.identity();
  mv.identity();
  mv.scale(0.8f,1.f,1.f);
  scene.setViewProject(mv,p,0,1,shMv);
  }

void InventoryRenderer::draw(Tempest::Encoder<CommandBuffer>& cmd) {
  Tempest::Matrix4x4 mv = Tempest::Matrix4x4::mkIdentity();
  mv.translate(0, 0, 0.5f);
  mv.scale(0.8f,1.f,1.f);

  auto& pso = Shaders::inst().inventory;
  for(auto& i:items) {
    cmd.setViewport(i.x,i.y,i.w,i.h);
    for(size_t r=0;r<i.mesh.nodesCount();++r) {
      auto  n = i.mesh.node(r);
      auto& m = n.material();

      if(auto s = n.mesh()) {
        auto sl = n.meshSlice();
        auto p  = mv;
        p.scale(1, 1, 0.25f);

        p.mul(n.position());
        p.mul(i.viewMat);

        Push push;
        push.mvp  = p;
        push.tint = i.tint;
        cmd.setBinding(0, *m.tex);
        cmd.setPushData(&push, sizeof(push));
        cmd.setPipeline(pso);
        cmd.draw(s->vbo, s->ibo, sl.first, sl.second);
        }
      }
    }
  }

void InventoryRenderer::reset(bool full) {
  items.clear();
  }

Tempest::Matrix4x4 InventoryRenderer::weaponView(const Tempest::Vec3* bbox, float screenRot) {
  float sz = (bbox[1]-bbox[0]).length();
  auto  mv = (bbox[1]+bbox[0])*0.5f;
  sz = std::min(2.f/sz, 0.1f);

  // like a melee weapon in drawItem
  Tempest::Matrix4x4 mat;
  mat.identity();
  mat.rotateOX(-45);
  mat.rotateOZ(90);
  for(int i=0; i<3; ++i){
    auto trX = mat.at(i,0);
    auto trY = mat.at(i,2);
    mat.set(i,0,trY);
    mat.set(i,2,trX);
    }
  mat.scale(sz);
  mat.translate(-mv);

  Tempest::Matrix4x4 view = Tempest::Matrix4x4::mkIdentity();
  view.rotateOZ(screenRot);
  view.mul(mat);
  return view;
  }

Tempest::Vec2 InventoryRenderer::toViewport(const Tempest::Matrix4x4& view, const Tempest::Vec3& p) {
  // the same chain as draw(): translate(0,0,0.5) * scale(0.8,1,1) * scale(1,1,0.25) * view, no projection
  Tempest::Matrix4x4 m = Tempest::Matrix4x4::mkIdentity();
  m.translate(0, 0, 0.5f);
  m.scale(0.8f,1.f,1.f);
  m.scale(1, 1, 0.25f);
  m.mul(view);
  Tempest::Vec3 v = p;
  m.project(v);
  return Tempest::Vec2((v.x+1.f)*0.5f, (v.y+1.f)*0.5f); // Vulkan: y down
  }

void InventoryRenderer::drawPointer(int hx, int hy, int size, std::string_view visual, const Tempest::Vec4& tint, bool flip) {
  auto mesh = Resources::loadMesh(visual);
  if(mesh==nullptr || size<=0)
    return;
  auto bbox = mesh->bbox();
  auto ext  = bbox[1]-bbox[0];
  auto c    = (bbox[1]+bbox[0])*0.5f;

  // the blade: the longest side of the box; the tip is the end farther from the grip (the origin of the model)
  Tempest::Vec3 a = c, b = c;
  if(ext.x>=ext.y && ext.x>=ext.z) { a.x = bbox[0].x; b.x = bbox[1].x; }
  else if(ext.y>=ext.z)            { a.y = bbox[0].y; b.y = bbox[1].y; }
  else                             { a.z = bbox[0].z; b.z = bbox[1].z; }
  Tempest::Vec3 tip = (b.length()>=a.length()) ? b : a;
  Tempest::Vec3 grp = (b.length()>=a.length()) ? a : b;
  if(flip)
    std::swap(tip,grp);

  // turn in the plane of the screen until the blade points to the top left (sign of the turn found by trying)
  auto dirOf = [&](float rot) {
    auto view = weaponView(bbox, rot);
    auto d    = toViewport(view,tip) - toViewport(view,grp);
    float l   = std::sqrt(d.x*d.x+d.y*d.y);
    return l>0 ? d/l : Tempest::Vec2(0,-1);
    };
  const Tempest::Vec2 want = Tempest::Vec2(-1,-1)/std::sqrt(2.f);
  auto  d0   = dirOf(0);
  float ang  = std::acos(std::clamp(d0.x*want.x+d0.y*want.y, -1.f, 1.f))*(180.f/3.14159265f);
  auto  dp   = dirOf(+ang);
  auto  dm   = dirOf(-ang);
  float rot  = (dp.x*want.x+dp.y*want.y >= dm.x*want.x+dm.y*want.y) ? +ang : -ang;

  auto  view = weaponView(bbox, rot);
  auto  t    = toViewport(view,tip);
  drawMesh(hx-int(t.x*float(size)), hy-int(t.y*float(size)), size, size, visual, rot, tint);
  }

void InventoryRenderer::drawMesh(int x, int y, int w, int h, std::string_view visual, float screenRot, const Tempest::Vec4& tint) {
  auto mesh = Resources::loadMesh(visual);
  if(mesh==nullptr)
    return;
  auto view = weaponView(mesh->bbox(), screenRot);

  Itm itm;
  itm.mesh = MeshObjects::Mesh(itmGroup,*mesh,0,0,0,false);
  itm.mesh.setObjMatrix(Matrix4x4::mkIdentity());
  itm.viewMat = view;
  itm.tint    = tint;
  itm.x    = x;
  itm.y    = y;
  itm.w    = w;
  itm.h    = h;
  items.push_back(std::move(itm));
  }

void InventoryRenderer::drawItem(int x, int y, int w, int h, const ::Item& item) {
  auto& itData = item.handle();
  if(auto mesh = Resources::loadMesh(itData.visual)) {
    auto     bbox = mesh->bbox();
    float    sz   = (bbox[1]-bbox[0]).length();
    auto     mv   = (bbox[1]+bbox[0])*0.5f;
    ItmFlags flg  = ItmFlags(item.mainFlag());

    mv = Vec3(mv.x,mv.y,mv.z);

    sz = 2.f/sz;
    if(sz>0.1f)
      sz=0.1f;

    Tempest::Matrix4x4 mat;
    mat.identity();

    float rotx = float(itData.inv_rot_x);
    float roty = float(itData.inv_rot_y);
    float rotz = float(itData.inv_rot_z);

    if(flg&(ITM_CAT_NF | ITM_CAT_FF | ITM_CAT_MUN)) {
      static const float invX = -45;
      static const float invY = 0;
      static const float invZ = 90;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if(flg&ITM_CAT_ARMOR) {
      static const float invX = 0;
      static const float invY = -90;
      static const float invZ = 180;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if(flg&ITM_CAT_RUNE) {
      static const float invX = 90;
      static const float invY = 0;
      static const float invZ = 90;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if((flg&ITM_CAT_MAGIC) || (ItmFlags(itData.flags)&ITM_RING)) {
      static const float invX = 200;
      static const float invY = 0;
      static const float invZ = 90;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if(flg&ITM_CAT_POTION) {
      static const float invX = 180;
      static const float invY = 0;
      static const float invZ = 0;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if(flg&ITM_CAT_FOOD) {
      static const float invX = 180;
      static const float invY = 0;
      static const float invZ = 45;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if(flg&ITM_CAT_DOCS) {
      static const float invX = 180;
      static const float invY = 90;
      static const float invZ = -90;
      mat.rotateOX(invX);
      mat.rotateOZ(invZ);
      mat.rotateOY(invY);
      }
    else if(flg&ITM_CAT_NONE) {
      static float invXa = 135;
      static float invYa = 90;
      static float invZa = 45;
      //invXa++;
      mat.rotateOX(invXa-rotx);
      mat.rotateOZ(invZa-rotz);
      mat.rotateOY(invYa-roty);
      } else {
      static const float invX = 180;
      static const float invY = -90;
      static const float invZ = -90;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }

    for(int i=0; i<3; ++i){
      auto trX = mat.at(i,0);
      auto trY = mat.at(i,2);
      mat.set(i,0,trY);
      mat.set(i,2,trX);
      }
    mat.scale(sz);
    mat.translate(-mv);

    Itm itm;
    itm.mesh = MeshObjects::Mesh(itmGroup,*mesh,itData.material,0,itData.material,false);
    itm.mesh.setObjMatrix(Matrix4x4::mkIdentity());
    itm.viewMat = mat;
    itm.x    = x;
    itm.y    = y;
    itm.w    = w;
    itm.h    = h;
    items.push_back(std::move(itm));
    }
  }
