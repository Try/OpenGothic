#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>

#include "staticobjects.h"

class Item;

class InventoryRenderer {
  public:
    InventoryRenderer(const RendererStorage& storage);

    void draw(Tempest::CommandBuffer& cmd, uint32_t imgId);
    void drawItem(int x, int y, int w, int h, const Item &item);

  private:
    struct PerFrame {
      Tempest::Matrix4x4 mvp;
      };

    struct Itm {
      StaticObjects::Mesh mesh;
      int x=0,y=0,w=0,h=0;
      };

    const RendererStorage&              storage;
    StaticObjects                       itmGroup;
    std::vector<Itm>                    items;
  };

