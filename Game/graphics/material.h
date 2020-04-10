#pragma once

#include <Tempest/Texture2d>
#include <zenload/zTypes.h>

class Material final {
  public:
    Material()=default;
    Material(const ZenLoad::zCMaterialData& m);

    enum ApphaFunc:uint8_t {
      InvalidAlpha =0,
      NoAlpha      =1,
      Transparent  =2,
      AdditiveLight=3,
      Multiply     =4,
      Multiply2    =5,
      Last
      };

    const Tempest::Texture2d* tex=nullptr;
    ApphaFunc                 alpha=NoAlpha;

    bool operator <(const Material& other) const;
  };

