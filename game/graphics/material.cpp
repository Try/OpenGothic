#include "material.h"

#include "utils/parser.h"
#include "resources.h"

using namespace Tempest;

static Tempest::Color toColor(glm::u8vec4 v) {
  Tempest::Color c(float(v.r)/255.f, float(v.g)/255.f, float(v.b)/255.f, float(v.a)/255.f);
  return c;
  }

Material::Material(const phoenix::material& m, bool enableAlphaTest) {
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

  if(m.texture_anim_map_mode!=phoenix::animation_mapping_mode::none && tex!=nullptr) {
    auto texAniMapDir = m.texture_anim_map_dir;
    if(texAniMapDir.x!=0.f)
      texAniMapDirPeriod.x = int(1.f/texAniMapDir.x);
    if(texAniMapDir.y!=0.f)
      texAniMapDirPeriod.y = int(1.f/texAniMapDir.y);
    }

  if(m.wave_mode!=phoenix::wave_mode_type::none)
    waveMaxAmplitude = m.wave_max_amplitude;

  if(m.environment_mapping!=0)
    envMapping = m.environment_mapping_strength;
  }

Material::Material(const phoenix::vob& vob) {
  tex = Resources::loadTexture(vob.visual_name);
  if(tex==nullptr && !vob.visual_name.empty())
    tex = Resources::loadTexture("DEFAULT.TGA");

  frames       = Resources::loadTextureAnim(vob.visual_name);

  texAniFPSInv = 1000/std::max<size_t>(frames.size(),1);
  alpha        = loadAlphaFunc(vob.visual_decal->alpha_func,phoenix::material_group::undefined,vob.visual_decal->alpha_weight,tex,true);
  alphaWeight  = float(vob.visual_decal->alpha_weight)/255.f;

  if(vob.visual_decal->texture_anim_fps>0)
    texAniFPSInv = uint64_t(1000.f/vob.visual_decal->texture_anim_fps); else
    texAniFPSInv = 1;
  }

Material::Material(const phoenix::c_particle_fx& src) {
  tex    = Resources::loadTexture(src.vis_name_s);
  frames = Resources::loadTextureAnim(src.vis_name_s);
  if(src.vis_tex_ani_fps>0)
    texAniFPSInv = uint64_t(1000.f/src.vis_tex_ani_fps); else
    texAniFPSInv = 1;
  //TODO: visTexAniIsLooping

  alpha = Parser::loadAlpha(src.vis_alpha_func_s);
  }

bool Material::operator < (const Material& other) const {
  auto a0 = alphaOrder(alpha,isGhost);
  auto a1 = alphaOrder(other.alpha,other.isGhost);

  if(a0<a1)
    return true;
  if(a0>a1)
    return false;
  return tex<other.tex;
  }

bool Material::operator >(const Material& other) const {
  auto a0 = alphaOrder(alpha,isGhost);
  auto a1 = alphaOrder(other.alpha,other.isGhost);

  if(a0>a1)
    return true;
  if(a0<a1)
    return false;
  return tex>other.tex;
  }

bool Material::operator ==(const Material& other) const {
  return tex==other.tex &&
         alpha==other.alpha &&
         alphaWeight==other.alphaWeight &&
         texAniMapDirPeriod==other.texAniMapDirPeriod &&
         texAniFPSInv==other.texAniFPSInv &&
         isGhost==other.isGhost &&
         waveMaxAmplitude==other.waveMaxAmplitude;
  }

bool Material::isSolid() const {
  return !isGhost && (alpha==Material::Solid || alpha==Material::AlphaTest);
  }

bool Material::isSceneInfoRequired() const {
  return isGhost || alpha==Material::Water || alpha==Material::Ghost;
  }

bool Material::isTesselated() const {
  // if(alpha!=Material::Water)
  //   return false;
  // return waveMaxAmplitude!=0.f;
  return (alpha==Material::Water);
  }

int Material::alphaOrder(AlphaFunc a, bool ghost) {
  if(ghost)
    return Ghost;
  return a;
  }

Material::AlphaFunc Material::loadAlphaFunc(phoenix::alpha_function zenAlpha,
                                            phoenix::material_group matGroup,
                                            uint8_t clrAlpha,
                                            const Tempest::Texture2d* tex,
                                            bool enableAlphaTest) {
  Material::AlphaFunc alpha = Material::AlphaFunc::AlphaTest;
  switch(zenAlpha) {
    case phoenix::alpha_function::blend:
      alpha = Material::AlphaFunc::Transparent;
      break;
    case phoenix::alpha_function::add:
      alpha = Material::AlphaFunc::AdditiveLight;
      break;
    case phoenix::alpha_function::sub:
      // FIXME: no such materials in game found
      alpha = Material::AlphaFunc::AdditiveLight;
      break;
    case phoenix::alpha_function::mul:
      alpha = Material::AlphaFunc::Multiply;
      break;
    case phoenix::alpha_function::mul2:
      alpha = Material::AlphaFunc::Multiply2;
      break;
    case phoenix::alpha_function::default_:
    case phoenix::alpha_function::none:
      alpha = Material::AlphaFunc::AlphaTest;
      break;
    }

  if(matGroup == phoenix::material_group::water)
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
    //    alpha = Material::AlphaFunc::Multiply;
    //   }
    }

  if(alpha == Material::AlphaFunc::AlphaTest && !enableAlphaTest) {
    alpha = Material::AlphaFunc::Solid;
    }
  return alpha;
  }

void Material::loadFrames(const phoenix::material& m) {
  frames = Resources::loadTextureAnim(m.texture);
  if(m.texture_anim_fps > 0)
    texAniFPSInv = uint64_t(1.0f / m.texture_anim_fps); else
    texAniFPSInv = 1;
  }
