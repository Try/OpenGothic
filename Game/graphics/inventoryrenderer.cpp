#include "inventoryrenderer.h"

#include "rendererstorage.h"
#include "world/item.h"
#include "light.h"

#include <Tempest/Application>
#include <Tempest/Encoder>

#include <game/inventory.h>

using namespace Tempest;

InventoryRenderer::InventoryRenderer(const RendererStorage &storage)
  :storage(storage),itmGroup(storage) {
  itmGroup.reserve(512,0);

  Light light;
  light.setColor(Vec3(0.f,0.f,0.f));
  itmGroup.setLight(light,Vec3(1.f,1.f,1.f));
  }

void InventoryRenderer::draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t imgId) {
  Tempest::Matrix4x4 mv;
  mv.identity();
  if(items.size()){
    // int w = items[0].w;
    // int h = items[0].h;
    // mv.perspective(45.0f, float(w)/float(h), 0.001f, 100.0f);
    mv.scale(0.8f,1.f,1.f);
    }
  Tempest::Matrix4x4 shMv[2];
  itmGroup.setModelView(mv,shMv,2);
  itmGroup.commitUbo(imgId,Resources::fallbackTexture());
  itmGroup.updateUbo(imgId);

  for(auto& i:items){
    cmd.setViewport(i.x,i.y,i.w,i.h);
    for(size_t r=0;r<i.mesh.nodesCount();++r){
      auto n = i.mesh.node(r);
      n.draw(cmd,storage.pObject,imgId);
      }
    }
  }

void InventoryRenderer::reset() {
  items.clear();
  }

void InventoryRenderer::drawItem(int x, int y, int w, int h, const Item& item) {
  auto& itData = *item.handle();
  if(auto mesh=Resources::loadMesh(itData.visual.c_str())) {
    float sz = (mesh->bbox[1]-mesh->bbox[0]).manhattanLength();
    auto  mv = (mesh->bbox[1]+mesh->bbox[0])*0.5f;
    Inventory::Flags flg = Inventory::Flags(item.mainFlag());

    sz = 2.f/sz;
    if(sz>0.1f)
      sz=0.1f;

    Tempest::Matrix4x4 mat;
    mat.identity();
    mat.scale(sz);

    float rotx = float(itData.inv_rotx);
    float roty = float(itData.inv_roty);
    float rotz = float(itData.inv_rotz);

    if(flg&(Inventory::ITM_CAT_NF | Inventory::ITM_CAT_FF | Inventory::ITM_CAT_MUN)) {
      static const float invX = -45;
      static const float invY = 0;
      static const float invZ = 90;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if(flg&Inventory::ITM_CAT_ARMOR) {
      static const float invX = 0;
      static const float invY = -90;
      static const float invZ = 180;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if(flg&Inventory::ITM_CAT_RUNE) {
      static const float invX = 90;
      static const float invY = 0;
      static const float invZ = 90;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if((flg&Inventory::ITM_CAT_MAGIC) || (Inventory::Flags(itData.flags)&Inventory::ITM_RING)) {
      static const float invX = 200;
      static const float invY = 0;
      static const float invZ = 90;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if(flg&Inventory::ITM_CAT_POTION) {
      static const float invX = 180;
      static const float invY = 0;
      static const float invZ = 0;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if(flg&Inventory::ITM_CAT_FOOD) {
      static const float invX = 180;
      static const float invY = 0;
      static const float invZ = 45;
      mat.rotateOX(invX+rotx);
      mat.rotateOZ(invZ+rotz);
      mat.rotateOY(invY+roty);
      }
    else if(flg&Inventory::ITM_CAT_DOCS) {
      static const float invX = 180;
      static const float invY = 90;
      static const float invZ = -90;
      mat.rotateOX(invX);
      mat.rotateOZ(invZ);
      mat.rotateOY(invY);
      }
    else if(flg&Inventory::ITM_CAT_NONE) {
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
    mat.translate(-mv.x,-mv.y,-mv.z);

    for(int i=0;i<3;++i){
      auto trX = mat.at(i,0);
      auto trY = mat.at(i,2);
      mat.set(i,0,trY);
      mat.set(i,2,trX);
      }

    for(int i=0;i<3;++i){
      mat.set(i,2,mat.at(i,2)*0.2f);
      }
    mat.set(3,2, 0.75f);//+itData.inv_zbias/1000.f);

    Itm itm;
    itm.mesh = itmGroup.get(*mesh,itData.material,0,itData.material);
    itm.mesh.setObjMatrix(mat);
    itm.x    = x;
    itm.y    = y;
    itm.w    = w;
    itm.h    = h;
    items.push_back(std::move(itm));
    }
  }
