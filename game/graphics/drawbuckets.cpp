#include "drawbuckets.h"

#include <Tempest/Vec>

#include "graphics/mesh/submesh/staticmesh.h"
#include "graphics/mesh/submesh/animmesh.h"

#include "gothic.h"

using namespace Tempest;

DrawBuckets::Id::Id(DrawBuckets* owner, size_t id):owner(owner), id(uint16_t(id)) {
  }

DrawBuckets::Id::Id(Id&& other) noexcept {
  std::swap(owner, other.owner);
  std::swap(id,    other.id);
  }

DrawBuckets::Id& DrawBuckets::Id::operator =(Id&& other) noexcept {
  std::swap(owner, other.owner);
  std::swap(id,    other.id);
  return *this;
  }

DrawBuckets::Id::~Id() {
  }


DrawBuckets::DrawBuckets() {
  }

DrawBuckets::Id DrawBuckets::alloc(const Material& mat, const StaticMesh& mesh) {
  for(size_t i=0; i<bucketsCpu.size(); ++i) {
    auto& b = bucketsCpu[i];
    if(b.staticMesh==&mesh && b.mat==mat)
      return Id(this, i);
    }

  Bucket bx;
  bx.staticMesh = &mesh;
  bx.mat        = mat;

  bucketsDurtyBit = true;
  bucketsCpu.emplace_back(std::move(bx));
  return Id(this, bucketsCpu.size()-1);
  }

DrawBuckets::Id DrawBuckets::alloc(const Material& mat, const AnimMesh& mesh) {
  for(size_t i=0; i<bucketsCpu.size(); ++i) {
    auto& b = bucketsCpu[i];
    if(b.animMesh==&mesh && b.mat==mat)
      return Id(this, i);
    }

  Bucket bx;
  bx.animMesh = &mesh;
  bx.mat      = mat;

  bucketsDurtyBit = true;
  bucketsCpu.emplace_back(std::move(bx));
  return Id(this, bucketsCpu.size()-1);
  }

const std::vector<DrawBuckets::Bucket>& DrawBuckets::buckets() {
  return bucketsCpu;
  }

void DrawBuckets::updateBindlessArrays() {
  if(!Gothic::inst().options().doBindless)
    return;

  std::vector<const Tempest::Texture2d*>     tex;
  std::vector<const Tempest::StorageBuffer*> vbo, ibo;
  std::vector<const Tempest::StorageBuffer*> morphId, morph;
  for(auto& i:bucketsCpu) {
    tex.push_back(i.mat.tex);
    if(i.staticMesh!=nullptr) {
      ibo    .push_back(&i.staticMesh->ibo8);
      vbo    .push_back(&i.staticMesh->vbo);
      morphId.push_back(i.staticMesh->morph.index);
      morph  .push_back(i.staticMesh->morph.samples);
      } else {
      ibo    .push_back(&i.animMesh->ibo8);
      vbo    .push_back(&i.animMesh->vbo);
      morphId.push_back(nullptr);
      morph  .push_back(nullptr);
      }
    }

  Resources::recycle(std::move(desc.tex));
  Resources::recycle(std::move(desc.vbo));
  Resources::recycle(std::move(desc.ibo));
  Resources::recycle(std::move(desc.morphId));
  Resources::recycle(std::move(desc.morph));

  auto& device = Resources::device();
  desc.tex     = device.descriptors(tex);
  desc.vbo     = device.descriptors(vbo);
  desc.ibo     = device.descriptors(ibo);
  desc.morphId = device.descriptors(morphId);
  desc.morph   = device.descriptors(morph);
  }

bool DrawBuckets::commit(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  if(!bucketsDurtyBit)
    return false;
  bucketsDurtyBit = false;

  std::vector<BucketGpu> bucket;
  for(auto& i:bucketsCpu) {
    BucketGpu bx;
    bx.texAniMapDirPeriod = i.mat.texAniMapDirPeriod;
    bx.waveMaxAmplitude   = i.mat.waveMaxAmplitude;
    bx.alphaWeight        = i.mat.alphaWeight;
    bx.envMapping         = i.mat.envMapping;
    if(i.staticMesh!=nullptr) {
      auto& bbox    = i.staticMesh->bbox.bbox;
      bx.bboxRadius = i.staticMesh->bbox.rConservative;
      bx.bbox[0]    = Vec4(bbox[0].x,bbox[0].y,bbox[0].z,0.f);
      bx.bbox[1]    = Vec4(bbox[1].x,bbox[1].y,bbox[1].z,0.f);
      if(i.staticMesh->morph.anim!=nullptr)
        bx.flags |= BK_MORPH;
      }
    else if(i.animMesh!=nullptr) {
      auto& bbox    = i.animMesh->bbox.bbox;
      bx.bboxRadius = i.animMesh->bbox.rConservative;
      bx.bbox[0]    = Vec4(bbox[0].x,bbox[0].y,bbox[0].z,0.f);
      bx.bbox[1]    = Vec4(bbox[1].x,bbox[1].y,bbox[1].z,0.f);
      bx.flags     |= BK_SKIN;
      }
    if(i.mat.alpha==Material::Solid)
      bx.flags |= BK_SOLID;
    if(i.mat.alpha==Material::Water)
      bx.flags |= BK_WATER;
    bucket.push_back(bx);
    }

  auto& device = Resources::device();
  Resources::recycle(std::move(bucketsGpu));
  bucketsGpu = device.ssbo(bucket);

  updateBindlessArrays();
  return true;
  }
