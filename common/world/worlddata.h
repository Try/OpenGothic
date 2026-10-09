#pragma once

#include <zenkit/World.hh>
#include "graphics/mesh/submesh/packedmesh.h"
#include "physics/dynamicworld.h"

// Original level data only. Never retain a GameSession, renderer, NPC or script VM here.
struct WorldData {
  const zenkit::VfsNode* source = nullptr;
  zenkit::GameVersion version = zenkit::GameVersion::GOTHIC_2;
  bool softwareRayTracing = false;
  zenkit::World world;
  std::unique_ptr<const PackedMesh> visual;
  std::shared_ptr<const DynamicWorld::Landscape> landscape;
  size_t cacheBytes = 0;
  };
