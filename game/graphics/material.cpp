#include "material.h"

#include "utils/parser.h"
#include "resources.h"

using namespace Tempest;

Material::Material(const phoenix::material& m, bool enableAlphaTest) {
  tex = Resources::loadTexture(m.texture);
  if(tex==nullptr && !m.texture.empty())
    tex = Resources::loadTexture("DEFAULT.TGA");

  loadFrames(m);

  alpha = loadAlphaFunc(m.alpha_func,m.group,tex,enableAlphaTest);

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
  alpha        = loadAlphaFunc(vob.visual_decal->alpha_func,phoenix::material_group::undefined,tex,true);

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
         texAniMapDirPeriod==other.texAniMapDirPeriod &&
         texAniFPSInv==other.texAniFPSInv &&
         isGhost==other.isGhost &&
         waveMaxAmplitude==other.waveMaxAmplitude;
  }

bool Material::isSolid() const {
  return !isGhost && (alpha==Material::Solid || alpha==Material::AlphaTest);
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
                                            const Tempest::Texture2d* tex,
                                            bool enableAlphaTest) {
  Material::AlphaFunc alpha = Material::AlphaFunc::AlphaTest;
  switch (zenAlpha) {
  case phoenix::alpha_function::blend:
    alpha = Material::AlphaFunc::Transparent;
    break;
  case phoenix::alpha_function::add:
    alpha = Material::AlphaFunc::AdditiveLight;
    break;
  case phoenix::alpha_function::mul: // TODO: originally, this was `sub` and `mul`
  case phoenix::alpha_function::mul2:
    alpha = Material::AlphaFunc::Multiply;
    break;
  default:
    alpha = Material::AlphaFunc::AlphaTest;
    break;
  }

  if (matGroup == phoenix::material_group::water)
    alpha = Material::AlphaFunc::Water;

  if (alpha == Material::AlphaFunc::AlphaTest || alpha == Material::AlphaFunc::Transparent) {
    if (tex != nullptr && tex->format() == Tempest::TextureFormat::DXT1) {
      alpha = Material::AlphaFunc::Solid;
    }
  }

  if (alpha == Material::AlphaFunc::AlphaTest && !enableAlphaTest) {
    alpha = Material::AlphaFunc::Solid;
  }
  return alpha;
}

void Material::loadFrames(const phoenix::material& m) {
  frames = Resources::loadTextureAnim(m.texture);
  if (m.texture_anim_fps > 0)
    texAniFPSInv = uint64_t(1.0f / m.texture_anim_fps);
  else
    texAniFPSInv = 1;
}
