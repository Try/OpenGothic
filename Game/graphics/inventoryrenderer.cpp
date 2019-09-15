#include "inventoryrenderer.h"

#include "rendererstorage.h"
#include "world/item.h"
#include "light.h"

#include <Tempest/Application>

#include <game/inventory.h>

using namespace Tempest;

InventoryRenderer::InventoryRenderer(const RendererStorage &storage)
  :storage(storage),itmGroup(storage) {
  itmGroup.reserve(512,0);

  Light light;
  light.setColor(Vec3(0.f,0.f,0.f));
  itmGroup.setLight(light,Vec3(1.f,1.f,1.f));
  }

void InventoryRenderer::draw(Tempest::CommandBuffer &cmd, uint32_t imgId) {
  Tempest::Matrix4x4 mv;
  mv.identity();
  if(items.size()){
    // int w = items[0].w;
    // int h = items[0].h;
    // mv.perspective(45.0f, float(w)/float(h), 0.001f, 100.0f);
    mv.scale(0.8f,1.f,1.f);
    }
  itmGroup.setModelView(mv,Tempest::Matrix4x4());
  itmGroup.commitUbo(imgId,Tempest::Texture2d());
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
  if(auto mesh=Resources::loadMesh(itData.visual)) {
    float sz = (mesh->bbox[1]-mesh->bbox[0]).manhattanLength();
    auto  mv = (mesh->bbox[1]+mesh->bbox[0])*0.5f;
    Inventory::Flags flg = Inventory::Flags(item.mainFlag());

    sz = 2.f/sz;
    Tempest::Matrix4x4 mat;
    mat.identity();
    mat.scale(sz);
    if(Inventory::Flags(itData.flags)&Inventory::ITM_RING)
      mat.scale(0.4f);

    if(flg&(Inventory::ITM_CAT_NF | Inventory::ITM_CAT_FF | Inventory::ITM_CAT_MUN |
            Inventory::ITM_CAT_ARMOR)) {
      static const float invX = -45;
      static const float invY = 0;
      static const float invZ = 90;
      mat.rotateOX(invX+itData.inv_rotx);
      mat.rotateOZ(invZ+itData.inv_rotz);
      mat.rotateOY(invY+itData.inv_roty);
      }
    else if(flg&Inventory::ITM_CAT_RUNE) {
      static const float invX = 90;
      static const float invY = 0;
      static const float invZ = 90;
      mat.rotateOX(invX+itData.inv_rotx);
      mat.rotateOZ(invZ+itData.inv_rotz);
      mat.rotateOY(invY+itData.inv_roty);
      }
    else if(flg&Inventory::ITM_CAT_MAGIC) {
      static const float invX = 200;
      static const float invY = 0;
      static const float invZ = 90;
      mat.rotateOX(invX+itData.inv_rotx);
      mat.rotateOZ(invZ+itData.inv_rotz);
      mat.rotateOY(invY+itData.inv_roty);
      }
    else if(flg&Inventory::ITM_CAT_POTION) {
      static const float invX = 180;
      static const float invY = 0;
      static const float invZ = 0;
      mat.rotateOX(invX+itData.inv_rotx);
      mat.rotateOZ(invZ+itData.inv_rotz);
      mat.rotateOY(invY+itData.inv_roty);
      }
    else if(flg&Inventory::ITM_CAT_FOOD) {
      static const float invX = 180;
      static const float invY = 0;
      static const float invZ = 45;
      mat.rotateOX(invX+itData.inv_rotx);
      mat.rotateOZ(invZ+itData.inv_rotz);
      mat.rotateOY(invY+itData.inv_roty);
      }
    else if(flg&Inventory::ITM_CAT_NONE) {
      static const float invX = 135;
      static const float invY = 90;
      static const float invZ = 45;
      mat.rotateOX(invX+itData.inv_rotx);
      mat.rotateOZ(invZ+itData.inv_rotz);
      mat.rotateOY(invY+itData.inv_roty);
      } else {
      static const float invX = 180;
      static const float invY = -90;
      static const float invZ = -90;
      mat.rotateOX(invX+itData.inv_rotx);
      mat.rotateOZ(invZ+itData.inv_rotz);
      mat.rotateOY(invY+itData.inv_roty);
      }
    mat.translate(-mv.x,-mv.y,-mv.z);

    for(int i=0;i<3;++i){
      auto x = mat.at(i,0);
      auto y = mat.at(i,2);
      mat.set(i,0,y);
      mat.set(i,2,x);
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
