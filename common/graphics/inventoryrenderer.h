#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>

#include "meshobjects.h"
#include "sceneglobals.h"
#include "visualobjects.h"

class Item;

class InventoryRenderer {
  public:
    InventoryRenderer();

    void draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd);

    void reset(bool full=false);
    void drawItem(int x, int y, int w, int h, const Item &item);

  private:
    struct PerFrame {
      Tempest::Matrix4x4 mvp;
      };

    struct Itm {
      MeshObjects::Mesh  mesh;
      Tempest::Matrix4x4 viewMat;
      int x=0, y=0, w=0, h=0;
      };

    SceneGlobals     scene;
    VisualObjects    visual;
    MeshObjects      itmGroup;
    std::vector<Itm> items;
  };

