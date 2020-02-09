#include "material.h"

#include "resources.h"

Material::Material(const ZenLoad::zCMaterialData& m) {
  tex   = Resources::loadTexture(m.texture);
  alpha = ApphaFunc(m.alphaFunc);
  if(alpha==InvalidAlpha) //Gothic1
    alpha = NoAlpha;
  //m.texAniMapDir;
  }

bool Material::operator < (const Material& other) const {
  if(alpha<other.alpha)
    return true;
  if(alpha>other.alpha)
    return false;
  return tex<other.tex;
  }
