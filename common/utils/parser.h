#pragma once

#include <Tempest/Vec>
#include "graphics/material.h"

namespace Parser {
  Tempest::Vec2       loadVec2 (std::string_view         src);
  Tempest::Vec3       loadVec3 (std::string_view         src);
  Material::AlphaFunc loadAlpha(std::string_view         src);
  }

