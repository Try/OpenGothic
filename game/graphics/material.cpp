#include "material.h"

#include <zenload/zCMaterial.h>

#include "utils/parser.h"
#include "resources.h"

using namespace Tempest;

Material::Material(const ZenLoad::zCMaterialData& m, bool enableAlphaTest) {
  tex = Resources::loadTexture(m.texture);
  if(tex==nullptr && !m.texture.empty())
    tex = Resources::loadTexture("DEFAULT.TGA");

  loadFrames(m);

  alpha = loadAlphaFunc(m.alphaFunc,m.matGroup,tex,enableAlphaTest);

  if(m.texAniMapMode!=0 && tex!=nullptr) {
    auto texAniMapDir = Parser::loadVec2(m.texAniMapDir);
    if(texAniMapDir.x!=0.f)
      texAniMapDirPeriod.x = int(1.f/texAniMapDir.x);
    if(texAniMapDir.y!=0.f)
      texAniMapDirPeriod.y = int(1.f/texAniMapDir.y);
    }

  if(m.waveMode!=0)
    waveMaxAmplitude = m.waveMaxAmplitude;

  if(m.environmentMapping!=0)
    envMapping = m.environmentalMappingStrength;
  }

Material::Material(const ZenLoad::zCVobData& vob) {
  tex = Resources::loadTexture(vob.visual);
  if(tex==nullptr && !vob.visual.empty())
    tex = Resources::loadTexture("DEFAULT.TGA");

  frames       = Resources::loadTextureAnim(vob.visual);

  texAniFPSInv = 1000/std::max<size_t>(frames.size(),1);
  alpha        = loadAlphaFunc(vob.visualChunk.zCDecal.decalAlphaFunc,ZenLoad::MaterialGroup::UNDEF,tex,true);

  if(vob.visualChunk.zCDecal.decalTexAniFPS>0)
    texAniFPSInv = uint64_t(1000.f/vob.visualChunk.zCDecal.decalTexAniFPS); else
    texAniFPSInv = 1;
  }

Material::Material(const Daedalus::GEngineClasses::C_ParticleFX& src) {
  tex    = Resources::loadTexture(src.visName_S.c_str());
  frames = Resources::loadTextureAnim(src.visName_S.c_str());
  if(src.visTexAniFPS>0)
    texAniFPSInv = uint64_t(1000.f/src.visTexAniFPS); else
    texAniFPSInv = 1;
  //TODO: visTexAniIsLooping

  alpha = Parser::loadAlpha(src.visAlphaFunc_S);
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

Material::AlphaFunc Material::loadAlphaFunc(int zenAlpha, uint8_t matGroup, const Tempest::Texture2d* tex, bool enableAlphaTest) {
  AlphaFunc alpha = AlphaTest;
  switch(zenAlpha) {
    case 0:
      // Gothic1
      alpha = AlphaTest;
      break;
    case 1:
      alpha = AlphaTest;
      break;
    case 2:
      alpha = Transparent;
      break;
    case 3:
      alpha = AdditiveLight;
      break;
    case 4:
      alpha = Multiply;
      break;
    case 5:
      alpha = Multiply2;
      break;
    default:
      alpha = AlphaTest;
      break;
    }

  if(matGroup==ZenLoad::MaterialGroup::WATER)
    alpha = Water;

  if(alpha==AlphaTest || alpha==Transparent) {
    if(tex!=nullptr && tex->format()==Tempest::TextureFormat::DXT1) {
      alpha = Solid;
      }
    }

  if(alpha==AlphaTest && !enableAlphaTest) {
    alpha = Solid;
    }
  return alpha;
  }

void Material::loadFrames(const ZenLoad::zCMaterialData& m) {
  frames = Resources::loadTextureAnim(m.texture);
  if(m.texAniFPS>0)
    texAniFPSInv = uint64_t(1000.f/m.texAniFPS); else
    texAniFPSInv = 1;
  }
