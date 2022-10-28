#include "parser.h"

#include <charconv>

using namespace Tempest;

Vec2 Parser::loadVec2(std::string_view src) {
  if(src=="=")
    return Vec2();

  float       v[2] = {};
  for(int i=0;i<2;++i) {
    // see https://en.cppreference.com/w/cpp/utility/from_chars
    auto result = std::from_chars(src.begin(), src.end(), v[i]);

    if(result.ec == std::errc::invalid_argument) {
      if(i==1)
        return Vec2(v[0],v[0]);
      }

    src = std::string_view {result.ptr, src.end()};
    }
  return Vec2(v[0],v[1]);
  }

Vec3 Parser::loadVec3(std::string_view src) {
  if(src=="=")
    return Vec3();

  float v[3] = {};
  for(int i=0;i<3;++i) {
    // see https://en.cppreference.com/w/cpp/utility/from_chars
    auto result = std::from_chars(src.begin(), src.end(), v[i]);

    if(result.ec == std::errc::invalid_argument) {
      if(i==1)
        return Vec3(v[0],v[0],v[0]);
      if(i==2)
        return Vec3(v[0],v[1],0.f);
    }

    src = std::string_view {result.ptr, src.end()};
    }
  return Vec3(v[0],v[1],v[2]);
  }

Material::AlphaFunc Parser::loadAlpha(std::string_view src) {
  if(src=="NONE")
    return Material::AlphaTest;
  if(src=="BLEND")
    return Material::Transparent;
  if(src=="ADD")
    return Material::AdditiveLight;
  if(src=="MUL")
    return Material::Multiply;
  return Material::Solid;
  }
