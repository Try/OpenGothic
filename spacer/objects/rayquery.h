#pragma once

#include <Tempest/Matrix4x4>

#include "worldedit.h"

class RayQuery {
  public:
    RayQuery(Tempest::Matrix4x4 view, Tempest::Matrix4x4 vp, Tempest::Point mpos, Tempest::Size wsize);
    RayQuery(const Tempest::Vec3 s, const Tempest::Vec3 e);

    void proceed(WorldEdit& world);

    WorldEdit::Vob* vob() const { return hitVob; }
    Tempest::Vec3   hitPos() const;

  private:
    void proceedLights(WorldEdit& world);
    void proceedLights(const Tempest::Matrix4x4& vp, float& rayT, WorldEdit::Vob*& ret, WorldEdit::Vob& v) const;

    auto validatePointer(const zenkit::VirtualObject* ptr, WorldEdit::Vob& v) const -> WorldEdit::Vob*;

    Tempest::Matrix4x4 viewProject;
    Tempest::Vec3 src, dst;
    Tempest::Point mpos;
    Tempest::Size  wsize;

    WorldEdit::Vob* hitVob = nullptr;
    float           hitFraction = {};
};
