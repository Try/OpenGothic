#pragma once

#include <phoenix/material.hh>

#include <zenload/zTypes.h>

namespace phoenix_compat {
  ZenLoad::zCMaterialData to_zenlib_material(const phoenix::material&);
}
