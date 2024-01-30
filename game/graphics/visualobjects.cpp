#include "visualobjects.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/animmesh.h"

using namespace Tempest;

VisualObjects::VisualObjects(const SceneGlobals& scene, const std::pair<Vec3, Vec3>& bbox)
    : scene(scene), drawMem(*this, scene), visGroup(bbox) {
  }

VisualObjects::~VisualObjects() {
  }

DrawStorage::Item VisualObjects::get(const StaticMesh& mesh, const Material& mat,
                                     size_t iboOff, size_t iboLen,
                                     bool staticDraw) {
  if(mat.tex==nullptr) {
    Log::e("no texture?!");
    return DrawStorage::Item();
    }
  const DrawStorage::Type bucket = (staticDraw ? DrawStorage::Static : DrawStorage::Movable);
  return drawMem.alloc(mesh, mat, iboOff, iboLen, bucket);
  }

DrawStorage::Item VisualObjects::get(const AnimMesh& mesh, const Material& mat,
                                     size_t iboOff, size_t iboLen,
                                     const InstanceStorage::Id& anim) {
  if(mat.tex==nullptr) {
    Log::e("no texture?!");
    return DrawStorage::Item();
    }
  return drawMem.alloc(mesh, mat, anim, iboOff, iboLen);
  }

DrawStorage::Item VisualObjects::get(const StaticMesh& mesh, const Material& mat,
                                     size_t iboOff, size_t iboLen, const PackedMesh::Cluster* cluster,
                                     DrawStorage::Type type) {
  return drawMem.alloc(mesh, mat, iboOff, iboLen, cluster, type);
  }

InstanceStorage::Id VisualObjects::alloc(size_t size) {
  return instanceMem.alloc(size);
  }

bool VisualObjects::realloc(InstanceStorage::Id& id, size_t size) {
  return instanceMem.realloc(id, size);
  }

const Tempest::StorageBuffer& VisualObjects::instanceSsbo() const {
  return instanceMem.ssbo();
  }

void VisualObjects::prepareUniforms() {
  drawMem.prepareUniforms();
  }

void VisualObjects::preFrameUpdate(uint8_t fId) {
  drawMem.preFrameUpdate(fId);
  }

void VisualObjects::visibilityPass (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t fId, int pass) {
  drawMem.visibilityPass(cmd, fId, pass);
  }

void VisualObjects::drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId) {
  drawMem.drawTranslucent(enc, fId);
  }

void VisualObjects::drawWater(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId) {
  drawMem.drawWater(enc, fId);
  }

void VisualObjects::drawGBuffer(Tempest::Encoder<CommandBuffer>& enc, uint8_t fId) {
  drawMem.drawGBuffer(enc, fId);
  }

void VisualObjects::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer) {
  drawMem.drawShadow(enc, fId, layer);
  }

void VisualObjects::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId) {
  drawMem.drawHiZ(enc, fId);
  }

void VisualObjects::notifyTlas(const Material& m, RtScene::Category cat) {
  scene.rtScene.notifyTlas(m,cat);
  }

void VisualObjects::prepareGlobals(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  bool sk = instanceMem.commit(cmd, fId);
  sk |= drawMem.commit(cmd, fId);
  if(!sk)
    return;
  drawMem.invalidateUbo();
  }

void VisualObjects::postFrameupdate() {
  instanceMem.join();
  }

bool VisualObjects::updateRtScene(RtScene& out) {
  if(!out.isUpdateRequired())
    return false;  
  drawMem.fillTlas(out);
  out.buildTlas();
  return true;
  }

void VisualObjects::dbgClusters(Tempest::Painter& p, Vec2 wsz) {
  drawMem.dbgDraw(p, wsz);
  }
