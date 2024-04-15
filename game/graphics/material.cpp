#include "material.h"

#include "utils/parser.h"
#include "resources.h"

using namespace Tempest;

static Tempest::Color toColor(glm::u8vec4 v) {
  Tempest::Color c(float(v.r)/255.f, float(v.g)/255.f, float(v.b)/255.f, float(v.a)/255.f);
  return c;
  }

Material::Material(const zenkit::Material& m, bool enableAlphaTest) {
  tex = Resources::loadTexture(m.texture);
  if(tex==nullptr) {
    if(!m.texture.empty()) {
      tex = Resources::loadTexture("DEFAULT.TGA");
      } else {
      tex = Resources::loadTexture(toColor(m.color));
      enableAlphaTest &= (m.color.a!=255);
      }
    }

  loadFrames(m);

  alpha = loadAlphaFunc(m.alpha_func,m.group,m.color.a,tex,enableAlphaTest);
  if(alpha==Water && m.name=="OWODWFALL_WATERFALL_01") {
    // NOTE: waterfall heuristics
    alpha = Solid;
    }

  if(m.texture_anim_map_mode!=zenkit::AnimationMapping::NONE && tex!=nullptr) {
    auto texAniMapDir = m.texture_anim_map_dir;
    if(texAniMapDir.x!=0.f)
      texAniMapDirPeriod.x = int(1.f/texAniMapDir.x);
    if(texAniMapDir.y!=0.f)
      texAniMapDirPeriod.y = int(1.f/texAniMapDir.y);
    }

  if(m.wave_mode!=zenkit::WaveMode::NONE)
    waveMaxAmplitude = m.wave_max_amplitude;

  if(m.environment_mapping!=0)
    ; // envMapping = m.environment_mapping_strength;
  }

Material::Material(const zenkit::VirtualObject& vob) {
  tex = Resources::loadTexture(vob.visual_name);
  if(tex==nullptr && !vob.visual_name.empty())
    tex = Resources::loadTexture("DEFAULT.TGA");
  loadFrames(vob.visual_name, vob.visual_decal->texture_anim_fps);

  alpha        = loadAlphaFunc(vob.visual_decal->alpha_func, zenkit::MaterialGroup::UNDEFINED, vob.visual_decal->alpha_weight, tex, true);
  alphaWeight  = float(vob.visual_decal->alpha_weight)/255.f;
  }

Material::Material(const zenkit::IParticleEffect& src) {
  tex = Resources::loadTexture(src.vis_name_s);
  loadFrames(src.vis_name_s, src.vis_tex_ani_fps);

  //TODO: visTexAniIsLooping
  alpha = Parser::loadAlpha(src.vis_alpha_func_s);
  }

bool Material::operator ==(const Material& other) const {
  return tex==other.tex &&
         frames==other.frames &&
         alpha==other.alpha &&
         alphaWeight==other.alphaWeight &&
         texAniMapDirPeriod==other.texAniMapDirPeriod &&
         texAniFPSInv==other.texAniFPSInv &&
         isGhost==other.isGhost &&
         waveMaxAmplitude==other.waveMaxAmplitude &&
         envMapping==other.envMapping;
  }

bool Material::isSolid() const {
  return !isGhost && (alpha==Material::Solid || alpha==Material::AlphaTest);
  }

bool Material::isForwardShading(AlphaFunc alpha) {
  return alpha!=Material::Solid && alpha!=Material::AlphaTest && alpha!=Material::Ghost;
  }

bool Material::isSceneInfoRequired(AlphaFunc alpha) {
  return alpha==Material::Water || alpha==Material::Ghost;
  }

bool Material::isShadowmapRequired(AlphaFunc alpha) {
  return isForwardShading(alpha) &&
         alpha!=Material::AdditiveLight && alpha!=Material::Multiply && alpha!=Material::Multiply2 &&
         alpha!=Material::Water;
  }

bool Material::isTextureInShadowPass(AlphaFunc alpha) {
  return (alpha==Material::AlphaTest);
  }

bool Material::isTesselated(AlphaFunc alpha) {
  // if(alpha!=Material::Water)
  //   return false;
  // return waveMaxAmplitude!=0.f;
  return (alpha==Material::Water);
  }

Material::AlphaFunc Material::loadAlphaFunc(zenkit::AlphaFunction zenAlpha,
                                            zenkit::MaterialGroup matGroup,
                                            uint8_t clrAlpha,
                                            const Tempest::Texture2d* tex,
                                            bool enableAlphaTest) {
  Material::AlphaFunc alpha = Material::AlphaFunc::AlphaTest;
  switch(zenAlpha) {
    case zenkit::AlphaFunction::BLEND:
      alpha = Material::AlphaFunc::Transparent;
      break;
    case zenkit::AlphaFunction::ADD:
      alpha = Material::AlphaFunc::AdditiveLight;
      break;
    case zenkit::AlphaFunction::SUBTRACT:
      // FIXME: no such materials in game found
      alpha = Material::AlphaFunc::AdditiveLight;
      break;
    case zenkit::AlphaFunction::MULTIPLY:
      alpha = Material::AlphaFunc::Multiply;
      break;
    case zenkit::AlphaFunction::MULTIPLY_ALT:
      alpha = Material::AlphaFunc::Multiply2;
      break;
    case zenkit::AlphaFunction::DEFAULT:
    case zenkit::AlphaFunction::NONE:
      alpha = Material::AlphaFunc::AlphaTest;
      break;
    }

  if(matGroup == zenkit::MaterialGroup::WATER)
    alpha = Material::AlphaFunc::Water;

  if(clrAlpha!=255 && alpha==Material::AlphaFunc::Solid) {
    alpha = Material::AlphaFunc::Transparent;
    }

  if(alpha==Material::AlphaFunc::AlphaTest || alpha==Material::AlphaFunc::Transparent) {
    if(tex!=nullptr && tex->format()==Tempest::TextureFormat::DXT1 && clrAlpha==255) {
      alpha = Material::AlphaFunc::Solid;
      }
    }

  if(alpha==Material::AlphaFunc::AlphaTest || alpha==Material::AlphaFunc::AdditiveLight) {
    // castle wall in G1 (OW_DIRTDECAL.TGA) has alpha==0, set it to mul for now
    // if(clrAlpha==0) {
    //   alpha = Material::AlphaFunc::Multiply;
    //   }
    }

  if(alpha == Material::AlphaFunc::AlphaTest && !enableAlphaTest) {
    alpha = Material::AlphaFunc::Solid;
    }
  return alpha;
  }

void Material::loadFrames(const zenkit::Material& m) {
  loadFrames(m.texture, m.texture_anim_fps);
  }

void Material::loadFrames(const std::string_view fr, float fps) {
  frames = Resources::loadTextureAnim(fr);
  if(frames.empty())
    return;
  if(fps > 0)
    texAniFPSInv = uint64_t(1000.0f / fps); else
    texAniFPSInv = 1000/std::max<size_t>(frames.size(),1);
  }
