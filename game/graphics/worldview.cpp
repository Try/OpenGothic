#include "worldview.h"

#include <Tempest/Application>

#include "graphics/mesh/submesh/packedmesh.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "gothic.h"

using namespace Tempest;

WorldView::WorldView(const World& world, const PackedMesh& wmesh)
  : owner(world),gSky(sGlobal,world,wmesh.bbox()),lights(sGlobal),visuals(sGlobal,wmesh.bbox()),
    objGroup(visuals),pfxGroup(*this,sGlobal,visuals),land(visuals,wmesh) {
  pfxGroup.resetTicks();
  }

WorldView::~WorldView() {
  // cmd buffers must not be in use
  Resources::device().waitIdle();
  }

const LightSource& WorldView::mainLight() const {
  return gSky.sunLight();
  }

const Tempest::Vec3& WorldView::ambientLight() const {
  return gSky.ambientLight();
  }

bool WorldView::isInPfxRange(const Vec3& pos) const {
  return pfxGroup.isInPfxRange(pos);
  }

void WorldView::tick(uint64_t /*dt*/) {
  auto pl = owner.player();
  if(pl!=nullptr) {
    pfxGroup.setViewerPos(pl->position());
    }
  }

void WorldView::preFrameUpdate(const Camera& camera, uint64_t tickCount, uint8_t fId) {
  const auto ldir = gSky.sunLight().dir();
  Tempest::Matrix4x4 shadow   [Resources::ShadowLayers];
  Tempest::Matrix4x4 shadowLwc[Resources::ShadowLayers];
  for(size_t i=0; i<Resources::ShadowLayers; ++i) {
    shadow   [i] = camera.viewShadow(ldir,i);
    shadowLwc[i] = camera.viewShadowLwc(ldir,i);
    }

  sGlobal.setSky(gSky);
  sGlobal.setViewProject(camera.view(),camera.projective(),camera.zNear(),camera.zFar(),shadow);
  sGlobal.setViewLwc(camera.viewLwc(),camera.projective(),shadowLwc);
  sGlobal.setViewVsm(camera.viewShadowVsm(ldir), camera.viewShadowVsmLwc(ldir));
  sGlobal.originLwc = camera.originLwc();
  sGlobal.setUnderWater(camera.isInWater());

  pfxGroup.tick(tickCount);
  lights.tick(tickCount);
  sGlobal.setTime(tickCount);
  sGlobal.commitUbo(fId);

  pfxGroup.preFrameUpdate(fId);
  visuals .preFrameUpdate(fId);
  }

void WorldView::prepareGlobals(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  sGlobal.prepareGlobals(cmd, fId);
  lights .prepareGlobals(cmd, fId);
  visuals.prepareGlobals(cmd, fId);
  }

void WorldView::setGbuffer(const Texture2d& diffuse, const Texture2d& norm) {
  sGlobal.gbufDiffuse = &diffuse;
  sGlobal.gbufNormals = &norm;
  sGlobal.setResolution(uint32_t(diffuse.w()),uint32_t(diffuse.h()));
  }

void WorldView::setShadowMaps(const Tempest::Texture2d* sh[]) {
  const Texture2d* shadow[Resources::ShadowLayers] = {};
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    if(sh[i]==nullptr || sh[i]->isEmpty())
      shadow[i] = &Resources::fallbackBlack(); else
      shadow[i] = sh[i];
  sGlobal.setShadowMap(shadow);
  }

void WorldView::setVirtualShadowMap(const Tempest::StorageImage&  pageData,
                                    const Tempest::StorageBuffer& pageList)  {
  sGlobal.setVirtualShadowMap(pageData, pageList);
  }

void WorldView::setSwRenderingImage(const Tempest::StorageImage& mainView) {
  sGlobal.setSwRenderingImage(mainView);
  }

void WorldView::setHiZ(const Tempest::Texture2d& hiZ) {
  sGlobal.setHiZ(hiZ);
  }

void WorldView::setSceneImages(const Tempest::Texture2d& clr, const Tempest::Texture2d& depthAux, const Tempest::ZBuffer& depthNative) {
  sGlobal.sceneColor = &clr;
  sGlobal.sceneDepth = &depthAux;
  sGlobal.zbuffer    = &textureCast<const Texture2d&>(depthNative);
  }

void WorldView::dbgLights(DbgPainter& p) const {
  lights.dbgLights(p);
  }

void WorldView::prepareSky(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t frameId) {
  gSky.prepareSky(cmd,frameId);
  }

void WorldView::prepareFog(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t frameId) {
  gSky.prepareFog(cmd,frameId);
  }

void WorldView::prepareIrradiance(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t frameId) {
  gSky.prepareIrradiance(cmd, frameId);
  }

void WorldView::prepareExposure(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t frameId) {
  gSky.prepareExposure(cmd, frameId);
  }

void WorldView::updateFrustrum(const Frustrum fr[]) {
  for(uint8_t i=0; i<SceneGlobals::V_Count; ++i)
    sGlobal.frustrum[i] = fr[i];
  }

void WorldView::visibilityPass(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int pass) {
  cmd.setDebugMarker("Visibility");
  visuals.visibilityPass(cmd, fId, pass);
  }

void WorldView::visibilityVsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  visuals.visibilityVsm(cmd, fId);
  }

void WorldView::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  visuals.drawHiZ(cmd,fId);
  }

void WorldView::drawShadow(Tempest::Encoder<CommandBuffer>& cmd, uint8_t fId, uint8_t layer) {
  visuals.drawShadow(cmd,fId,layer);
  pfxGroup.drawShadow(cmd,fId,layer);
  }

void WorldView::drawVsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  visuals.drawVsm(cmd, fId);
  }

void WorldView::drawSwr(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  visuals.drawSwr(cmd, fId);
  }

void WorldView::drawGBuffer(Tempest::Encoder<CommandBuffer>& cmd, uint8_t fId) {
  visuals.drawGBuffer(cmd, fId);
  pfxGroup.drawGBuffer(cmd, fId);
  }

void WorldView::drawSky(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  gSky.drawSky(cmd,fId);
  }

void WorldView::drawSunMoon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  gSky.drawSunMoon(cmd,fId);
  }

void WorldView::drawWater(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  visuals.drawWater(cmd,fId);
  }

void WorldView::drawTranslucent(Tempest::Encoder<CommandBuffer>& cmd, uint8_t fId) {
  visuals.drawTranslucent(cmd,fId);
  pfxGroup.drawTranslucent(cmd, fId);
  }

void WorldView::drawFog(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  gSky.drawFog(cmd,fId);
  }

void WorldView::drawLights(Tempest::Encoder<CommandBuffer>& cmd, uint8_t fId) {
  lights.draw(cmd, fId);
  }

MeshObjects::Mesh WorldView::addView(std::string_view visual, int32_t headTex, int32_t teethTex, int32_t bodyColor) {
  if(auto mesh=Resources::loadMesh(visual))
    return MeshObjects::Mesh(objGroup,*mesh,headTex,teethTex,bodyColor,false);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::addView(const ProtoMesh* mesh) {
  if(mesh!=nullptr) {
    bool staticDraw = false;
    return MeshObjects::Mesh(objGroup,*mesh,0,0,0,staticDraw);
    }
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::addItmView(std::string_view visual, int32_t material) {
  if(auto mesh=Resources::loadMesh(visual))
    return MeshObjects::Mesh(objGroup,*mesh,material,0,0,true);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::addAtachView(const ProtoMesh::Attach& visual, const int32_t version) {
  return MeshObjects::Mesh(objGroup,visual,version,false);
  }

MeshObjects::Mesh WorldView::addStaticView(const ProtoMesh* mesh, bool staticDraw) {
  if(mesh!=nullptr)
    return MeshObjects::Mesh(objGroup,*mesh,0,0,0,staticDraw);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::addStaticView(std::string_view visual) {
  if(auto mesh=Resources::loadMesh(visual))
    return MeshObjects::Mesh(objGroup,*mesh,0,0,0,true);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::addDecalView(const zenkit::VisualDecal& vob) {
  if(auto mesh=Resources::decalMesh(vob))
    return MeshObjects::Mesh(objGroup,*mesh,0,0,0,true);
  return MeshObjects::Mesh();
  }

LightGroup::Light WorldView::addLight(const zenkit::VLight& vob) {
  auto l = lights.add(vob);
  l.setTimeOffset(owner.tickCount());
  return l;
  }

LightGroup::Light WorldView::addLight(std::string_view preset) {
  auto l = lights.add(preset);
  l.setTimeOffset(owner.tickCount());
  return l;
  }

void WorldView::dbgClusters(Tempest::Painter& p, Vec2 wsz) {
  visuals.dbgClusters(p, wsz);
  }

bool WorldView::updateLights() {
  const int64_t now = owner.time().timeInDay().toInt();
  gSky.updateLight(now);
  if(!lights.updateLights())
    return false;
  visuals.prepareLigtsUniforms();
  return true;
  }

bool WorldView::updateRtScene() {
  if(!Gothic::inst().options().doRayQuery)
    return false;
  if(!visuals.updateRtScene(sGlobal.rtScene))
    return false;
  // assume device-idle, if RT scene was recreated
  lights.prepareRtUniforms();
  return true;
  }

void WorldView::prepareUniforms() {
  // wait before update all descriptors, cmd buffers must not be in use
  Resources::device().waitIdle();
  sGlobal.skyLut = &gSky.skyLut();
  sGlobal.lights = &lights.lightsSsbo();
  lights.prepareUniforms();
  gSky.prepareUniforms();
  pfxGroup.prepareUniforms();
  visuals.prepareUniforms();
  }

void WorldView::postFrameupdate() {
  visuals.postFrameupdate();
  }
