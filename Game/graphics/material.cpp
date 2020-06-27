#include "material.h"

#include "resources.h"

using namespace Tempest;

Material::Material(const ZenLoad::zCMaterialData& m) {
  tex   = Resources::loadTexture(m.texture);
  alpha = ApphaFunc(m.alphaFunc);

  if(m.alphaFunc==0) //Gothic1
    alpha = AlphaTest;

  if(alpha>LastGothic) {
    alpha = InvalidAlpha;
    }

  if(alpha==AlphaTest || alpha==Transparent) {
    if(tex!=nullptr && tex->format()==Tempest::TextureFormat::DXT1) {
      alpha = Solid;
      }
    }

  if(m.texAniMapMode!=0 && tex!=nullptr) {
    auto texAniMapDir = loadVec2(m.texAniMapDir);
    if(texAniMapDir.x!=0.f)
      texAniMapDirPeriod.x = int(1.f/texAniMapDir.x);
    if(texAniMapDir.y!=0.f)
      texAniMapDirPeriod.y = int(1.f/texAniMapDir.y);
    }
  }

Material::Material(const Daedalus::GEngineClasses::C_ParticleFX& src) {
  tex = Resources::loadTexture(src.visName_S.c_str());
  if(src.visAlphaFunc_S=="NONE")
    alpha = AlphaTest;
  if(src.visAlphaFunc_S=="BLEND")
    alpha = Transparent;
  if(src.visAlphaFunc_S=="ADD")
    alpha = AdditiveLight;
  if(src.visAlphaFunc_S=="MUL")
    alpha = Multiply;
  }

Vec2 Material::loadVec2(const std::string& src) {
  if(src=="=")
    return Vec2();

  float       v[2] = {};
  const char* str  = src.c_str();
  for(int i=0;i<2;++i) {
    char* next=nullptr;
    v[i] = std::strtof(str,&next);
    if(str==next) {
      if(i==1)
        return Vec2(v[0],v[0]);
      }
    str = next;
    }
  return Vec2(v[0],v[1]);
  }

bool Material::operator < (const Material& other) const {
  auto a0 = alphaOrder(alpha);
  auto a1 = alphaOrder(other.alpha);

  if(a0<a1)
    return true;
  if(a0>a1)
    return false;
  return tex<other.tex;
  }

bool Material::operator ==(const Material& other) const {
  return tex==other.tex &&
         alpha==other.alpha &&
         texAniMapDirPeriod==other.texAniMapDirPeriod;
  }

int Material::alphaOrder(Material::ApphaFunc a) {
  if(a==Solid)
    return -1;
  return a;
  }
