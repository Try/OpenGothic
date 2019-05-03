#pragma once

#include <Tempest/Device>
#include <Tempest/Shader>

#include "physics/dynamicworld.h"

#include "graphics/sky/sky.h"
#include "graphics/staticobjects.h"
#include "graphics/landscape.h"
#include "light.h"
#include "posepool.h"

class World;
class RendererStorage;

class WorldView {
  public:
    WorldView(const World &world, const ZenLoad::PackedMesh& wmesh, const RendererStorage& storage);
    ~WorldView();

    void initPipeline(uint32_t w, uint32_t h);

    Tempest::Matrix4x4        viewProj(const Tempest::Matrix4x4 &view) const;
    const Tempest::Matrix4x4& projective() const { return proj; }

    const Light&              mainLight() const;

    bool needToUpdateCmd() const;
    void updateCmd (const World &world, const Tempest::Texture2d &shadow, const Tempest::RenderPass &shadowPass);
    void updateUbo (const Tempest::Matrix4x4 &view, const Tempest::Matrix4x4 &shadow, uint32_t imgId);
    void drawShadow(Tempest::CommandBuffer &cmd, Tempest::FrameBuffer &fbo, const Tempest::RenderPass &pass, uint32_t imgId);
    void draw      (Tempest::CommandBuffer &cmd, Tempest::FrameBuffer &fbo, const Tempest::RenderPass &pass, uint32_t imgId);
    void resetCmd  ();

    StaticObjects::Mesh getView      (const std::string& visual, int32_t headTex, int32_t teethTex, int32_t bodyColor);
    StaticObjects::Mesh getStaticView(const std::string& visual, int32_t material);
    void addStatic(const ZenLoad::zCVobData &vob);

    std::shared_ptr<Pose> get(const Skeleton* s,const Animation::Sequence *sq,uint64_t sT,std::shared_ptr<Pose>& prev);
    std::shared_ptr<Pose> get(const Skeleton* s,const Animation::Sequence *sq,const Animation::Sequence *sq1,uint64_t sT,std::shared_ptr<Pose>& prev);

    void updateAnimation(uint64_t tickCount);

  private:
    const World&            owner;
    const RendererStorage&  storage;

    Light                   sun;
    Sky                     sky;
    Landscape               land;
    StaticObjects           vobGroup;
    StaticObjects           objGroup;
    StaticObjects           itmGroup;
    bool                    nToUpdateCmd=true;

    Tempest::Matrix4x4      proj;

    struct StaticObj {
      StaticObjects::Mesh mesh;
      DynamicWorld::Item  physic;
      };

    PosePool                            animPool;
    std::vector<Tempest::CommandBuffer> cmdMain,cmdShadow;
    std::vector<StaticObj>              objStatic;

    void prebuiltCmdBuf(const World &world, const Tempest::Texture2d &shadowMap, const Tempest::RenderPass &shadowPass);
  };
