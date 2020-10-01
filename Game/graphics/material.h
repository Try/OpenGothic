#pragma once

#include <Tempest/Texture2d>
#include <daedalus/DaedalusStdlib.h>
#include <zenload/zTypes.h>

class Material final {
  public:
    Material()=default;
    Material(const ZenLoad::zCMaterialData& m, bool enableAlphaTest);
    Material(const ZenLoad::zCVobData& vob);
    Material(const Daedalus::GEngineClasses::C_ParticleFX &src);

    enum ApphaFunc:uint8_t {
      InvalidAlpha    = 0,
      AlphaTest       = 1,
      Transparent     = 2,
      AdditiveLight   = 3,
      Multiply        = 4,
      Multiply2       = 5,
      LastGothic,

      FirstOpenGothic,
      Solid,
      Water,
      Last
      };

    const Tempest::Texture2d* tex=nullptr;
    std::vector<const Tempest::Texture2d*> frames;
    ApphaFunc                 alpha=AlphaTest;
    Tempest::Point            texAniMapDirPeriod;
    uint64_t                  texAniFPSInv=1;

    bool operator <  (const Material& other) const;
    bool operator >  (const Material& other) const;
    bool operator == (const Material& other) const;

    int  alphaOrder() const { return alphaOrder(alpha); }

  private:
    static int alphaOrder(ApphaFunc a);

    void          loadFrames(const ZenLoad::zCMaterialData& m);
    Tempest::Vec2 loadVec2(const std::string& src);
  };

