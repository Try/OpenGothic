#pragma once

#include <Tempest/Device>
#include <Tempest/Shader>

#include <phoenix/world/vob_tree.hh>

#include "graphics/mesh/landscape.h"
#include "graphics/meshobjects.h"
#include "graphics/mesh/protomesh.h"
#include "graphics/pfx/pfxobjects.h"
#include "graphics/sky/sky.h"
#include "lightsource.h"
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

    bool isInPfxRange(const Tempest::Vec3& pos) const;

    Tempest::Signal<void(const Tempest::AccelerationStructure* tlas)> onTlasChanged;

    void tick(uint64_t dt);

    void preFrameUpdate(const Camera& camera, uint64_t tickCount, uint8_t fId);
    void prepareGlobals(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);

    void setGbuffer(const Tempest::Texture2d& diffuse,
                    const Tempest::Texture2d& norm);
    void setShadowMaps (const Tempest::Texture2d* shadow[]);
    void setHiZ(const Tempest::Texture2d& hiZ);
    void setSceneImages(const Tempest::Texture2d& clr, const Tempest::Texture2d& depthAux, const Tempest::ZBuffer& depthNative);

    void setupUbo();
    void setupTlas(const Tempest::AccelerationStructure* tlas);

    void dbgLights    (DbgPainter& p) const;
    void prepareSky   (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void prepareFog   (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void updateLight();

    void visibilityPass(const Frustrum fr[]);
    void drawHiZ        (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawShadow     (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId, uint8_t layer);
    void drawGBuffer    (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawSky        (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawSunMoon    (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawFog        (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawWater      (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawLights     (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);

    MeshObjects::Mesh   addView      (std::string_view visual, int32_t headTex, int32_t teethTex, int32_t bodyColor);
    MeshObjects::Mesh   addView      (const ProtoMesh* visual);
    MeshObjects::Mesh   addItmView   (std::string_view visual, int32_t material);
    MeshObjects::Mesh   addAtachView (const ProtoMesh::Attach& visual, const int32_t version);
    MeshObjects::Mesh   addStaticView(const ProtoMesh* visual, bool staticDraw = false);
    MeshObjects::Mesh   addStaticView(std::string_view visual);
    MeshObjects::Mesh   addDecalView (const phoenix::vob& vob);

    const Tempest::AccelerationStructure& landscapeTlas();
    const SceneGlobals&  sceneGlobals() const { return sGlobal; }
    const Sky&           sky() const { return gSky; }

  private:
    const World&  owner;
    SceneGlobals  sGlobal;
    Sky           gSky;
    VisualObjects visuals;

    MeshObjects   objGroup;
    PfxObjects    pfxGroup;
    Landscape     land;

    Tempest::AccelerationStructure tlasLand;

    bool needToUpdateCmd(uint8_t frameId) const;
    void invalidateCmd();

  friend class LightGroup::Light;
  friend class PfxEmitter;
  friend class TrlObjects;
  };
