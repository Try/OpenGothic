#pragma once

#include <phoenix/material.hh>
#include <phoenix/softskin_mesh.hh>

#include <zenload/zTypes.h>

namespace phoenix_compat {
  ZenLoad::zCMaterialData to_zenlib_material(const phoenix::material&);

  phoenix::bounding_box get_total_aabb(const phoenix::softskin_mesh&);

  ZenLoad::PackedSkeletalMesh pack_softskin_mesh(const phoenix::softskin_mesh&);
}
