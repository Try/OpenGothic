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

    enum AlphaFunc:uint8_t {
      Solid,
      AlphaTest,
      Water,
      Ghost,
      Transparent,
      AdditiveLight,
      Multiply,
      Multiply2,
      };

    const Tempest::Texture2d* tex=nullptr;
    std::vector<const Tempest::Texture2d*> frames;
    AlphaFunc                 alpha        = AlphaTest;
    Tempest::Point            texAniMapDirPeriod;
    uint64_t                  texAniFPSInv = 1;
    bool                      isGhost      = false;

    bool operator <  (const Material& other) const;
    bool operator >  (const Material& other) const;
    bool operator == (const Material& other) const;

    bool isSolid() const;
    int  alphaOrder() const { return alphaOrder(alpha,isGhost); }

  private:
    static int alphaOrder(AlphaFunc a, bool ghost);

    static AlphaFunc loadAlphaFunc(int zenAlpha, uint8_t mat, const Tempest::Texture2d* tex, bool enableAlphaTest);

    void          loadFrames(const ZenLoad::zCMaterialData& m);
  };

