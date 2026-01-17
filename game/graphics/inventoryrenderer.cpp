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

        cmd.setBinding(0, *m.tex);
        cmd.setPushData(p);
        cmd.setPipeline(pso);
        cmd.draw(s->vbo, s->ibo, sl.first, sl.second);
        }
      }
    }
  }

void InventoryRenderer::reset(bool full) {
  items.clear();
  }

void InventoryRenderer::drawItem(int x, int y, int w, int h, const ::Item& item) {
  auto& itData = item.handle();
  if(auto mesh = Resources::loadMesh(itData.visual)) {
    float    sz  = (mesh->bbox[1]-mesh->bbox[0]).length();
    auto     mv  = (mesh->bbox[1]+mesh->bbox[0])*0.5f;
    ItmFlags flg = ItmFlags(item.mainFlag());

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
