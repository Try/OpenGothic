#include "parser.h"
#include "utils/string_frm.h"

using namespace Tempest;

Vec2 Parser::loadVec2(std::string_view src) {
  if(src=="=")
    return Vec2();

  float v[2] = {};
  string_frm str(src);
  for(int i=0; i<2; ++i) {
    char* next = nullptr;
    v[i] = std::strtof(str.c_str(),&next);
    if(str==next) {
      if(i==1)
        return Vec2(v[0],v[0]);
      }
    str = next;
    }
  return Vec2(v[0],v[1]);
  }

Vec3 Parser::loadVec3(std::string_view src) {
  if(src=="=")
    return Vec3();

  float v[3] = {};
  string_frm str(src);
  for(int i=0; i<3; ++i) {
    char* next=nullptr;
    v[i] = std::strtof(str.c_str(),&next);
    if(str==next) {
      if(i==1)
        return Vec3(v[0],v[0],v[0]);
      if(i==2)
        return Vec3(v[0],v[1],0.f);
      }
    str = next;
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
