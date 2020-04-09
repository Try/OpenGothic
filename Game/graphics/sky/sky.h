#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Texture2d>
#include <array>

#include "graphics/meshobjects.h"
#include "graphics/ubochain.h"
#include "resources.h"

class RendererStorage;
class World;

class Sky final {
  public:
    using Vertex=Resources::VertexFsq;

    Sky(const RendererStorage &storage);

    void setWorld(const World &world);

    bool needToUpdateCommands(uint32_t frameId) const;
    void setAsUpdated        (uint32_t frameId);

    void setMatrix(uint32_t frameId,const Tempest::Matrix4x4& mat);
    void commitUbo(uint32_t frameId);

    void draw       (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t frameId, const World &world);
    void setLight   (const std::array<float,3>& l);
    void setDayNight(float dayF);

    struct Layer final {
      const Tempest::Texture2d* texture=nullptr;
      };

    struct State final {
      Layer lay[2];
      };

  private:
    struct UboGlobal {
      Tempest::Matrix4x4 mvp;
      float              color[4]={};
      float              dxy0[2]={};
      float              dxy1[2]={};
      float              sky [3]={};
      float              night  =1.0;
      };

    static std::array<float,3>    mkColor(uint8_t r,uint8_t g,uint8_t b);
    const Tempest::Texture2d*     skyTexture(const char* name, bool day, size_t id);
    const Tempest::Texture2d*     implSkyTexture(const char* name, bool day, size_t id);

    const RendererStorage&        storage;
    UboGlobal                     uboCpu;
    UboChain<UboGlobal>           uboGpu;
    Tempest::VertexBuffer<Vertex> vbo;
    std::vector<bool>             nToUpdate;

    State                         day, night;
    const World*                  world=nullptr;

    MeshObjects::Mesh             skymesh;

    static std::array<float,3>    color;
  };
