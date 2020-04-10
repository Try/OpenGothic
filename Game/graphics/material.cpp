#include "material.h"

#include "resources.h"

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
  //m.texAniMapDir;
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

int Material::alphaOrder(Material::ApphaFunc a) {
  if(a==Solid)
    return -1;
  return a;
  }
