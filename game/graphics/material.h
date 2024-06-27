#pragma once

#include <Tempest/Texture2d>

#include <zenkit/Material.hh>
#include <zenkit/world/VobTree.hh>
#include <zenkit/DaedalusScript.hh>
#include <zenkit/addon/daedalus.hh>

class Material final {
  public:
    Material()=default;
    Material(const zenkit::Material& m, bool enableAlphaTest);
    Material(const zenkit::VisualDecal& decal);
    Material(const zenkit::IParticleEffect &src);

    enum AlphaFunc:uint8_t {
      Solid,
      AlphaTest,
      Water,
      Ghost,
      Multiply,
      Multiply2,
      Transparent,
      AdditiveLight,
      };

    const Tempest::Texture2d* tex=nullptr;
    std::vector<const Tempest::Texture2d*> frames;
    AlphaFunc                 alpha            = AlphaTest;
    float                     alphaWeight      = 1;
    Tempest::Point            texAniMapDirPeriod;
    uint64_t                  texAniFPSInv     = 1;
    bool                      isGhost          = false;
    float                     waveMaxAmplitude = 0;
    float                     envMapping       = 0;

    bool operator == (const Material& other) const;

    bool isSolid() const;

    bool hasFrameAnimation()     const { return !frames.empty() && texAniFPSInv!=0;    }
    bool hasUvAnimation()        const { return texAniMapDirPeriod!=Tempest::Point(0); }
    bool isTesselated()          const { return isTesselated(alpha);                   }
    bool isForwardShading()      const { return isForwardShading(alpha);               }
    bool isSceneInfoRequired()   const { return isSceneInfoRequired(alpha);            }
    bool isShadowmapRequired()   const { return isShadowmapRequired(alpha);            }
    bool isTextureInShadowPass() const { return isTextureInShadowPass(alpha);          }

    static bool isTesselated(AlphaFunc alpha);
    static bool isForwardShading(AlphaFunc alpha);
    static bool isSceneInfoRequired(AlphaFunc alpha);
    static bool isShadowmapRequired(AlphaFunc alpha);
    static bool isTextureInShadowPass(AlphaFunc alpha);

  private:
    static AlphaFunc loadAlphaFunc(zenkit::AlphaFunction zenAlpha, zenkit::MaterialGroup matGroup,
                                   uint8_t alpha, const Tempest::Texture2d* tex, bool enableAlphaTest);
    void             loadFrames(const zenkit::Material& m);
    void             loadFrames(const std::string_view fr, float fps);
  };

