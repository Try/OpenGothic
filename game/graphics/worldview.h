#pragma once

#include <Tempest/Device>
#include <Tempest/Shader>

#include <zenkit/world/VobTree.hh>

#include "graphics/mesh/landscape.h"
#include "graphics/meshobjects.h"
#include "graphics/mesh/protomesh.h"
#include "graphics/pfx/pfxobjects.h"
#include "graphics/sky/sky.h"
#include "lightsource.h"
#include "lightgroup.h"
#include "sceneglobals.h"
#include "visualobjects.h"

class World;
class Camera;
class ParticleFx;
class PackedMesh;

class WorldView {
  public:
    WorldView(const World &world, const PackedMesh& wmesh);
    ~WorldView();

    const LightSource&        mainLight() const;
    const Tempest::Vec3&      ambientLight() const;
    std::pair<Tempest::Vec3, Tempest::Vec3> bbox() const;

    bool isInPfxRange(const Tempest::Vec3& pos) const;

    void tick(uint64_t dt);

    void resetRendering();

    void preFrameUpdate(const Camera& camera, uint64_t tickCount, uint8_t fId);
    void postFrameupdate();

    void prepareGlobals(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t fId);

    void setGbuffer(const Tempest::Texture2d& diffuse,
                    const Tempest::Texture2d& norm);
    void setShadowMaps (const Tempest::Texture2d* shadow[]);
    void setVirtualShadowMap(const Tempest::ZBuffer& pageData,
                             const Tempest::StorageImage& pageTbl,
                             const Tempest::StorageImage& pageHiZ,
                             const Tempest::StorageBuffer& pageList);
    void setSwRenderingImage(const Tempest::StorageImage& mainView);
    void setHiZ(const Tempest::Texture2d& hiZ);
    void setSceneImages(const Tempest::Texture2d& clr, const Tempest::Texture2d& depthAux, const Tempest::ZBuffer& depthNative);

    void dbgLights      (DbgPainter& p) const;

    bool updateLights();
    bool updateRtScene();

    void updateFrustrum (const Frustrum fr[]);
    void visibilityPass (Tempest::Encoder<Tempest::CommandBuffer>& cmd, int pass);
    void visibilityVsm  (Tempest::Encoder<Tempest::CommandBuffer>& cmd);

    void drawHiZ        (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawShadow     (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t frameId, uint8_t layer);
    void drawVsm        (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawSwr        (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawGBuffer    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t frameId);
    void drawWater      (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t frameId);

    MeshObjects::Mesh   addView      (std::string_view visual, int32_t headTex, int32_t teethTex, int32_t bodyColor);
    MeshObjects::Mesh   addView      (const ProtoMesh* visual);
    MeshObjects::Mesh   addItmView   (std::string_view visual, int32_t material);
    MeshObjects::Mesh   addAtachView (const ProtoMesh::Attach& visual, const int32_t version);
    MeshObjects::Mesh   addStaticView(const ProtoMesh* visual, bool staticDraw = false);
    MeshObjects::Mesh   addStaticView(std::string_view visual);
    MeshObjects::Mesh   addDecalView (const zenkit::VisualDecal& vob);
    LightGroup::Light   addLight     (const zenkit::VLight& vob);
    LightGroup::Light   addLight     (std::string_view preset);

    void                dbgClusters(Tempest::Painter& p, Tempest::Vec2 wsz);

    const SceneGlobals& sceneGlobals() const { return sGlobal; }
    const Sky&          sky() const { return gSky; }
    const Landscape&    landscape() const { return land; }
    const LightGroup&   lights() const { return gLights; }

  private:
    const World&  owner;
    std::pair<Tempest::Vec3, Tempest::Vec3> aabb;
    SceneGlobals  sGlobal;
    Sky           gSky;
    LightGroup    gLights;
    VisualObjects visuals;

    MeshObjects   objGroup;
    PfxObjects    pfxGroup;
    Landscape     land;

    bool needToUpdateCmd(uint8_t frameId) const;
    void invalidateCmd();

  friend class LightGroup::Light;
  friend class PfxEmitter;
  friend class TrlObjects;
  };
