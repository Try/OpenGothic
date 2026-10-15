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
    bool isEmpty() const { return items.empty(); }
    void drawItem(int x, int y, int w, int h, const Item &item);
    // Diathic: a model by its file name, turned like a weapon in the inventory and then by 'screenRot' degrees
    // in the plane of the screen; tint: rgb multiplies the colours, a adds a red glow
    void drawMesh(int x, int y, int w, int h, std::string_view visual, float screenRot, const Tempest::Vec4& tint);
    // Diathic: a weapon as a mouse pointer: its tip (the end of the longest side away from the grip, 'flip' = the
    // other end) exactly at (hx,hy), the blade pointing to the top left, in a size x size box
    void drawPointer(int hx, int hy, int size, std::string_view visual, const Tempest::Vec4& tint, bool flip = false);

  private:
    struct PerFrame {
      Tempest::Matrix4x4 mvp;
      };

    struct Push {
      Tempest::Matrix4x4 mvp;
      Tempest::Vec4      tint;
      };

    struct Itm {
      MeshObjects::Mesh  mesh;
      Tempest::Matrix4x4 viewMat;
      Tempest::Vec4      tint = Tempest::Vec4(1,1,1,0);
      int x=0, y=0, w=0, h=0;
      };

    static Tempest::Matrix4x4 weaponView(const Tempest::Vec3* bbox, float screenRot);
    static Tempest::Vec2      toViewport(const Tempest::Matrix4x4& view, const Tempest::Vec3& p); // 0..1 in the box

    SceneGlobals     scene;
    VisualObjects    visual;
    MeshObjects      itmGroup;
    std::vector<Itm> items;
  };

