#pragma once

#include <Tempest/Vec>
#include <daedalus/ZString.h>
#include "graphics/material.h"

namespace Parser {
  Tempest::Vec2       loadVec2 (const Daedalus::ZString& src);
  Tempest::Vec2       loadVec2 (std::string_view         src);
  Tempest::Vec3       loadVec3 (const Daedalus::ZString& src);
  Material::AlphaFunc loadAlpha(const Daedalus::ZString& src);
  };

