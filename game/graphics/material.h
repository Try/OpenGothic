#pragma once

#include <Tempest/Texture2d>

#include <phoenix/material.hh>
#include <phoenix/world/vob_tree.hh>
#include <phoenix/ext/daedalus_classes.hh>

class Material final {
  public:
    Material()=default;
    Material(const phoenix::material& m, bool enableAlphaTest);
    Material(const phoenix::vob& vob);
    Material(const phoenix::c_particle_fx &src);

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
    AlphaFunc                 alpha            = AlphaTest;
    Tempest::Point            texAniMapDirPeriod;
    uint64_t                  texAniFPSInv     = 1;
    bool                      isGhost          = false;
    float                     waveMaxAmplitude = 0;
    float                     envMapping       = 0;

    bool operator <  (const Material& other) const;
    bool operator >  (const Material& other) const;
    bool operator == (const Material& other) const;

    bool isSolid() const;
    bool isTesselated() const;
    int  alphaOrder() const { return alphaOrder(alpha,isGhost); }

  private:
    static int alphaOrder(AlphaFunc a, bool ghost);

    static AlphaFunc loadAlphaFunc(phoenix::alpha_function zenAlpha, phoenix::material_group matGroup, const Tempest::Texture2d* tex, bool enableAlphaTest);
    void             loadFrames(const phoenix::material& m);
  };

