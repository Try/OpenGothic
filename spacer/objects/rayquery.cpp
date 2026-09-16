#include "rayquery.h"

#include "assets.h"

using namespace Tempest;

RayQuery::RayQuery(Tempest::Matrix4x4 v, Tempest::Matrix4x4 vp,
                              Tempest::Point mpos, Tempest::Size wsize)
  :viewProject(vp), mpos(mpos), wsize(wsize) {
  auto vInv  = v;
  auto vpInv = vp;
  vInv.inverse();
  vpInv.inverse();

  Tempest::Vec2 pos = {mpos.x/float(wsize.w), mpos.y/float(wsize.h)};
  pos = 2.f*pos - 1.f;

  Vec3 dst = {pos.x, pos.y, 1};
  vpInv.project(dst);

  Vec3 src = {pos.x, pos.y, 0};
  vInv.project(src);

  this->src = src;
  this->dst = dst;
  }

RayQuery::RayQuery(const Tempest::Vec3 s, const Tempest::Vec3 e)
  :src(s), dst(e) {
  }

Vec3 RayQuery::hitPos() const {
  return src*(1.0-hitFraction) + dst*hitFraction;
  }

void RayQuery::proceed(WorldEdit& world) {
  auto ret  = world.dynamic().ray(src, dst);
  auto uptr = reinterpret_cast<zenkit::VirtualObject*>(ret.uptr);

  hitVob      = validatePointer(uptr, world.root());
  hitFraction = ret.hitFraction;

  proceedLights(world);
  }

void RayQuery::proceedLights(WorldEdit& world) {
  if(wsize.isEmpty())
    return;
  proceedLights(viewProject, hitFraction, hitVob, world.root());
  }

void RayQuery::proceedLights(const Tempest::Matrix4x4& vp, float& rayT, WorldEdit::Vob*& ret, WorldEdit::Vob& v) const {
  if(v.get()!=nullptr && v.get()->type==zenkit::VirtualObjectType::zCVobLight) {
    auto& vob  = *v.get();
    auto  pos  = Vec3(vob.position.x,vob.position.y,vob.position.z);
    auto  ndc  = pos;
    vp.project(ndc);

    ndc = (ndc*0.5 + 0.5);
    ndc *= Vec3(wsize.w, wsize.h, 1);

    const int spriteSize = Assets::inst().im.pointLight.w();
    if(ndc.z>0 && Vec2(ndc.x - mpos.x, ndc.y - mpos.y).quadLength() < spriteSize*spriteSize) {
      auto dir     = (dst - src);
      auto forward = Vec3(vp[0][2], vp[1][2], vp[2][2]);
      forward = Vec3::normalize(forward);

      float bT = Vec3::dotProduct(pos - src, forward) / Vec3::dotProduct(dir, forward);
      if(0<bT && bT < rayT) {
        rayT = bT;
        ret  = &v;
        }
      }
    }

  for(size_t i=0; i<v.size(); ++i) {
    proceedLights(vp, rayT, ret, v[i]);
    }
  }

WorldEdit::Vob* RayQuery::validatePointer(const zenkit::VirtualObject* ptr, WorldEdit::Vob& v) const {
  if(ptr==v.get())
    return &v;

  for(size_t i=0; i<v.size(); ++i) {
    if(auto n = validatePointer(ptr, v[i]))
      return n;
    }
  return nullptr;
  }
