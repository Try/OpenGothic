#include "inventoryrenderer.h"

#include "world/objects/item.h"
#include "game/inventory.h"
#include "graphics/mesh/protomesh.h"
#include "lightsource.h"

using namespace Tempest;

InventoryRenderer::InventoryRenderer()
  :visual(scene),itmGroup(visual) {
  LightSource light;
  light.setColor(Vec3(0.f,0.f,0.f));
  scene.ambient = Vec3(1.f,1.f,1.f);
  scene.sun     = light;

  Tempest::Matrix4x4 mv, shMv[2];
  mv.identity();
  mv.scale(0.8f,1.f,1.f);
  scene.setModelView(mv,shMv,2);
  }

void InventoryRenderer::draw(Tempest::Encoder<CommandBuffer>& cmd, uint8_t fId) {
  scene.commitUbo(fId);
  visual.preFrameUpdate(fId);

  for(auto& i:items) {
    cmd.setViewport(i.x,i.y,i.w,i.h);
    for(size_t r=0;r<i.mesh.nodesCount();++r) {
      auto n = i.mesh.node(r);
      n.draw(cmd,fId);
      }
    }
  }

void InventoryRenderer::reset() {
  items.clear();
  }

void InventoryRenderer::drawItem(int x, int y, int w, int h, const Item& item) {
  auto& itData = item.handle();
  if(auto mesh=Resources::loadMesh(itData.visual.c_str())) {
    float    sz  = (mesh->bbox[1]-mesh->bbox[0]).length();
    auto     mv  = (mesh->bbox[1]+mesh->bbox[0])*0.5f;
    ItmFlags flg = ItmFlags(item.mainFlag());

    mv = Vec3(mv.x,mv.y,mv.z);

    sz = 2.f/sz;
    if(sz>0.1f)
      sz=0.1f;

    Tempest::Matrix4x4 mat;
    mat.identity();
    mat.scale(sz);

    float rotx = float(itData.inv_rotx);
    float roty = float(itData.inv_roty);
    float rotz = float(itData.inv_rotz);

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
      static const float invX = 135;
      static const float invY = 90;
      static const float invZ = 45;
      mat.rotateOX(invX-rotx);
      mat.rotateOZ(invZ-rotz);
      mat.rotateOY(invY-roty);
      } else {
      static const float invX = 180;
      static const float invY = -90;
      static const float invZ = -90;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }

    for(int i=0;i<3;++i){
      auto trX = mat.at(i,0);
      auto trY = mat.at(i,2);
      mat.set(i,0,trY);
      mat.set(i,2,trX);
      }
    mat.translate(-mv);

    for(int i=0;i<3;++i){
      mat.set(i,2,mat.at(i,2)*0.2f);
      }
    mat.set(3,2, 0.75f);//+itData.inv_zbias/1000.f);

    Itm itm;
    itm.mesh = MeshObjects::Mesh(itmGroup,*mesh,itData.material,0,itData.material,true);
    itm.mesh.setObjMatrix(mat);
    itm.x    = x;
    itm.y    = y;
    itm.w    = w;
    itm.h    = h;
    items.push_back(std::move(itm));
    }
  }
