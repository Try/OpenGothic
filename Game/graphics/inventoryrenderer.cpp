#include "inventoryrenderer.h"

#include "rendererstorage.h"
#include "world/item.h"

InventoryRenderer::InventoryRenderer(const RendererStorage &storage)
  :storage(storage),itmGroup(storage) {
  itmGroup.reserve(512,0);
  }

void InventoryRenderer::draw(Tempest::CommandBuffer &cmd, uint32_t imgId) {
  Tempest::Matrix4x4 mv;
  mv.identity();
  if(items.size()){
    int w = items[0].w;
    int h = items[0].h;
    mv.perspective(45.0f, float(w)/float(h), 0.05f, 100.0f);
    }
  itmGroup.setModelView(mv,Tempest::Matrix4x4());
  itmGroup.commitUbo(imgId,Tempest::Texture2d());
  itmGroup.updateUbo(imgId);
  //itmGroup.draw(cmd,imgId);

  for(auto& i:items){
    // cmd.setViewport(i.x,i.y,i.w,i.h);
    for(size_t r=0;r<i.mesh.nodesCount();++r){
      auto n = i.mesh.node(r);
      n.draw(cmd,storage.pObject,imgId);
      }
    }
  items.clear();
  }

void InventoryRenderer::drawItem(int x, int y, int w, int h, const Item& item) {
  /*
  auto& itData = *item.handle();
  if(auto mesh=Resources::loadMesh(itData.visual)) {
    Tempest::Matrix4x4 mat;
    mat.identity();
    mat.scale(0.01f);
    mat.rotateOZ(45);

    Itm itm;
    itm.mesh = itmGroup.get(*mesh,itData.material,0,itData.material);
    itm.mesh.setObjMatrix(mat);
    itm.x    = x;
    itm.y    = y;
    itm.w    = w;
    itm.h    = h;
    items.push_back(std::move(itm));
    }*/
  }
