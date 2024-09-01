#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/DescriptorSet>
#include <Tempest/StorageBuffer>
#include <vector>

#include "sceneglobals.h"

class VisualObjects;
class DrawBuckets;
class DrawClusters;

class DrawCommands {
  public:
    enum Type : uint8_t {
      Landscape,
      Static,
      Movable,
      Animated,
      Pfx,
      Morph,
      };

    struct DrawCmd {
      const Tempest::RenderPipeline* pMain        = nullptr;
      const Tempest::RenderPipeline* pShadow      = nullptr;
      const Tempest::RenderPipeline* pHiZ         = nullptr;
      Type                           type         = Type::Landscape;

      Material::AlphaFunc            alpha        = Material::Solid;
      uint32_t                       firstPayload = 0;
      uint32_t                       maxPayload   = 0;

      // bindless only
      Tempest::DescriptorSet         desc[SceneGlobals::V_Count];

      // bindfull only
      uint32_t                       bucketId     = 0;
      Tempest::DescriptorSet         descFr[Resources::MaxFramesInFlight][SceneGlobals::V_Count];

      bool                           isForwardShading() const;
      bool                           isShadowmapRequired() const;
      bool                           isSceneInfoRequired() const;
      bool                           isTextureInShadowPass() const;
      bool                           isBindless() const;
      bool                           isMeshShader() const;
      };

    DrawCommands(VisualObjects& owner, DrawBuckets& buckets, DrawClusters& clusters, const SceneGlobals& scene);
    ~DrawCommands();

    const DrawCmd& operator[](size_t i) const { return cmd[i]; }

    bool     commit();
    uint16_t commandId(const Material& m, Type type, uint32_t bucketId);
    void     addClusters(uint16_t cmdId, uint32_t meshletCount);

    void     prepareUniforms();
    void     prepareLigtsUniforms();

    void     updateUniforms(uint8_t fId);
    void     updateTasksUniforms();

    void     visibilityPass(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int pass);
    void     visibilityVsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

    void     drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void     drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, SceneGlobals::VisCamera viewId, Material::AlphaFunc func);

    void     drawVsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void     drawSwr(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

  private:
    enum TaskLinkpackage : uint8_t {
      T_Scene    = 0,
      T_Payload  = 1,
      T_Instance = 2,
      T_Bucket   = 3,
      T_Indirect = 4,
      T_Clusters = 5,
      T_HiZ      = 6,
      };

    enum UboLinkpackage : uint8_t {
      L_Scene    = 0,
      L_Payload  = 1,
      L_Instance = 2,
      L_Pfx      = L_Instance,
      L_Bucket   = 3,
      L_Ibo      = 4,
      L_Vbo      = 5,
      L_Diffuse  = 6,
      L_Sampler  = 7,
      L_Shadow0  = 8,
      L_Shadow1  = 9,
      L_MorphId  = 10,
      L_Morph    = 11,
      L_SceneClr = 12,
      L_GDepth   = 13,
      };

    struct IndirectCmd {
      uint32_t vertexCount   = 0;
      uint32_t instanceCount = 0;
      uint32_t firstVertex   = 0;
      uint32_t firstInstance = 0;
      uint32_t writeOffset   = 0;
      };

    struct TaskCmd {
      SceneGlobals::VisCamera viewport = SceneGlobals::V_Main;
      Tempest::DescriptorSet  desc;
      };

    struct View {
      Tempest::DescriptorSet descInit;
      Tempest::StorageBuffer visClusters, indirectCmd;
      };

    void                     updateCommandUniforms();
    void                     updateVsmUniforms();

    VisualObjects&           owner;
    DrawBuckets&             buckets;
    DrawClusters&            clusters;
    const SceneGlobals&      scene;

    std::vector<TaskCmd>     tasks;
    std::vector<DrawCmd>     cmd;
    std::vector<DrawCmd*>    ord;
    Tempest::DescriptorSet   vsmDesc;
    Tempest::DescriptorSet   swrDesc;
    bool                     cmdDurtyBit = false;

    View                     views[SceneGlobals::V_Count];
  };
