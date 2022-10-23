#include "particlefx.h"

#include "utils/parser.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

ParticleFx::ParticleFx(const Material& mat, const phoenix::vob& vob) {
  dbgName          = vob.visual_name;

  ppsValue         = -1;
  lspPartAvg       = 1000;
  dirMode          = ParticleFx::Dir::Dir;
  visTexColorStart = Vec3(255,255,255);
  visTexColorEnd   = Vec3(255,255,255);
  visSizeStart     = Vec2(2.f*vob.visual_decal->dimension.x,
                          2.f*vob.visual_decal->dimension.y);
  visOrientation   = Orientation::None;

  visMaterial      = mat;

  shpFOR           = Frame::World;

  dirFOR           = Frame::World;
  dirAngleElev     = 90;
  visSizeEndScale  = 1;
  visAlphaStart    = 1;
  visAlphaEnd      = 1;
  visYawAlign      = vob.sprite_camera_facing_mode==phoenix::sprite_alignment::yaw;
  visZBias         = vob.bias!=0;

  useEmittersFOR   = true;
  }

ParticleFx::ParticleFx(const phoenix::c_particle_fx &src, std::string_view name)
  :dbgName(name) {
  ppsValue            = std::max(0.f,src.pps_value);
  ppsScaleKeys        = loadArr(src.pps_scale_keys_s);
  ppsIsLooping        = src.pps_is_looping!=0;
  ppsIsSmooth         = src.pps_is_smooth!=0;
  ppsFPS              = src.pps_fps>0 ? src.pps_fps : 1.f;
  ppsCreateEm         = Gothic::inst().loadParticleFx(src.pps_create_em_s);
  ppsCreateEmDelay    = uint64_t(src.pps_create_em_delay);

  shpType             = loadEmitType(src.shp_type_s);
  shpFOR              = loadFrameType(src.shp_for_s);
  shpOffsetVec        = Parser::loadVec3(src.shp_offset_vec_s);
  shpDistribType      = loadDistribType(src.shp_distrib_type_s);
  shpDistribWalkSpeed = src.shp_distrib_walk_speed;
  shpIsVolume         = src.shp_is_volume!=0;
  shpDim              = Parser::loadVec3(src.shp_dim_s);
  shpMesh_S           = src.shp_mesh_s;
  shpMesh             = Resources::loadEmiterMesh(src.shp_mesh_s);
  shpMeshRender       = src.shp_mesh_render_b!=0;
  shpScaleKeys        = loadArr(src.shp_scale_keys_s);
  shpScaleIsLooping   = src.shp_scale_is_looping!=0;
  shpScaleIsSmooth    = src.shp_scale_is_smooth!=0;
  shpScaleFPS         = src.shp_scale_fps;

  dirMode             = loadDirType(src.dir_mode_s);
  dirFOR              = loadFrameType(src.dir_for_s);
  dirModeTargetFOR    = loadFrameType(src.dir_mode_target_for_s);
  dirModeTargetPos    = Parser::loadVec3(src.dir_mode_target_pos_s);
  dirAngleHead        = src.dir_angle_head;
  dirAngleHeadVar     = src.dir_angle_head_var;
  dirAngleElev        = src.dir_angle_elev;
  dirAngleElevVar     = src.dir_angle_elev_var;

  velAvg              = src.vel_avg;
  velVar              = src.vel_var;
  lspPartAvg          = src.lsp_part_avg;
  lspPartVar          = src.lsp_part_var;

  flyGravity          = Parser::loadVec3(src.fly_gravity_s);
  flyCollDet          = src.fly_colldet_b!=0;

  visMaterial         = Material(src);
  visOrientation      = loadOrientation(src.vis_orientation_s);
  visTexIsQuadPoly    = src.vis_tex_is_quadpoly;
  visTexAniFPS        = src.vis_tex_ani_fps;
  visTexAniIsLooping  = src.vis_tex_ani_is_looping;
  visTexColorStart    = Parser::loadVec3(src.vis_tex_color_start_s);
  visTexColorEnd      = Parser::loadVec3(src.vis_tex_color_end_s);
  visSizeStart        = Parser::loadVec2(src.vis_size_start_s);
  visSizeEndScale     = src.vis_size_end_scale;
  visAlphaStart       = src.vis_alpha_start/255.f;
  visAlphaEnd         = src.vis_alpha_end/255.f;

  trlFadeSpeed       = src.trl_fade_speed;
  trlTexture         = loadTexture(src.trl_texture_s);
  trlWidth           = src.trl_width;

  mrkFadeSpeed       = src.mrk_fades_peed;
  mrkTexture         = loadTexture(src.mrkt_exture_s);
  mrkSize            = src.mrk_size;

  flockMode           = src.flock_mode;
  flockStrength       = src.flock_strength;

  useEmittersFOR      = src.use_emitters_for!=0;

  timeStartEnd_S      = src.time_start_end_s;
  m_bIsAmbientPFX     = src.m_bis_ambient_pfx!=0;

  prefferedTime       = calcPrefferedTimeSingle();

  // visMaterial.alpha = Material::Solid;
  }

ParticleFx::ParticleFx(const ParticleFx& proto, const VisualFx::Key& key)
  :ParticleFx(proto) {
  shpDim              = key.pfx_shpDim.value_or(shpDim);
  shpIsVolume         = key.pfx_shpIsVolumeChg!=0;

  if(key.pfx_shpScaleFPS>0)
    shpScaleFPS       = key.pfx_shpScaleFPS;

  shpDistribWalkSpeed = key.pfx_shpDistribWalkSpeed;
  shpOffsetVec        = key.pfx_shpOffsetVec.value_or(shpOffsetVec);
  if(!key.pfx_shpDistribType_S.empty())
    shpDistribType    = loadDistribType(key.pfx_shpDistribType_S);

  if(!key.pfx_dirMode_S.empty())
    dirMode           = loadDirType(key.pfx_dirMode_S);

  if(!key.pfx_dirFOR_S.empty())
    dirFOR            = loadFrameType(key.pfx_dirFOR_S);

  if(!key.pfx_dirModeTargetFOR_S.empty())
    dirModeTargetFOR  = loadFrameType(key.pfx_dirModeTargetFOR_S);

  if(!key.pfx_dirModeTargetPos_S.empty())
    dirModeTargetPos  = Parser::loadVec3(key.pfx_dirModeTargetPos_S);

  if(key.pfx_velAvg>0)
    velAvg            = key.pfx_velAvg;

  if(key.pfx_lspPartAvg>0)
    lspPartAvg        = key.pfx_lspPartAvg;

  if(key.pfx_visAlphaStart>0)
    visAlphaStart     = key.pfx_visAlphaStart;
  }

uint64_t ParticleFx::maxLifetime() const {
  return uint64_t(lspPartAvg+lspPartVar);
  }

uint64_t ParticleFx::effectPrefferedTime() const {
  auto v0 = prefferedTime;
  auto v1 = ppsCreateEm==nullptr ? 0 : ppsCreateEmDelay+ppsCreateEm->effectPrefferedTime();
  return std::max(v0,v1);
  }

float ParticleFx::maxPps() const {
  if(ppsValue<0)
    return 1; // permanent particles

  if(ppsScaleKeys.size()==0)
    return ppsValue;

  float v = 0;
  for(auto i:ppsScaleKeys)
    v = std::max(i,v);
  return v*ppsValue;
  }

float ParticleFx::shpScale(uint64_t time) const {
  return fetchScaleKey(time,shpScaleKeys,shpScaleFPS,
                       shpScaleIsSmooth!=0,shpScaleIsLooping!=0);
  }

float ParticleFx::ppsScale(uint64_t time) const {
  float v = fetchScaleKey(time,ppsScaleKeys,ppsFPS,ppsIsSmooth!=0,ppsIsLooping!=0);
  if(v<0)
    return 0.f;
  return v;
  }

uint64_t ParticleFx::calcPrefferedTimeSingle() const {
  uint64_t ret = calcPrefferedTimeSingle(ppsScaleKeys,ppsFPS);
  //ret = std::max<uint64_t>(1,ret);
  return ret;
  }

uint64_t ParticleFx::calcPrefferedTimeSingle(const KeyList& k, float fps) {
  if(k.size()==0 || fps<=0.f)
    return 0;

  float raw = float(k.size())*1000.f/fps;
  if(raw>float(std::numeric_limits<uint32_t>::max()))
    return 0;

  auto  sec = std::max<uint64_t>(1,uint64_t(std::ceil(raw)));
  return sec;
  }

const Tempest::Texture2d* ParticleFx::loadTexture(std::string_view src) {
  auto view = Resources::loadTexture(src);
  if(view==nullptr)
    view = &Resources::fallbackBlack();
  return view;
  }

ParticleFx::KeyList ParticleFx::loadArr(std::string_view src) {
  std::vector<float> v;
  const char* str  = src.data();
  for(int i=0;;++i) {
    char* next=nullptr;
    float f = std::strtof(str,&next);
    if(str==next) {
      return v;
      }
    v.push_back(f);
    str = next;
    }
  }

ParticleFx::EmitterType ParticleFx::loadEmitType(std::string_view src) {
  if(src=="POINT")
    return EmitterType::Point;
  if(src=="LINE")
    return EmitterType::Line;
  if(src=="BOX")
    return EmitterType::Box;
  if(src=="CIRCLE")
    return EmitterType::Circle;
  if(src=="SPHERE")
    return EmitterType::Sphere;
  if(src=="MESH")
    return EmitterType::Mesh;
  return EmitterType::Point;
  }

ParticleFx::Frame ParticleFx::loadFrameType(std::string_view src) {
  if(src=="OBJECT")
    return Frame::Object;
  if(src=="object")
    return Frame::Node; // look like "object"(low case) is not valid token - should be interpreted differently
  if(src=="WORLD")
    return Frame::World;
  return Frame::World;
  }

ParticleFx::Distribution ParticleFx::loadDistribType(std::string_view src) {
  if(src=="RAND" || src=="RANDOM")
    return Distribution::Rand;
  if(src=="DIR")
    return Distribution::Dir;
  if(src=="UNIFORM")
    return Distribution::Uniform;
  if(src=="WALK" || src=="WALKW")
    return Distribution::Walk;
  return Distribution::Rand;
  }

ParticleFx::Dir ParticleFx::loadDirType(std::string_view src) {
  if(src=="RAND")
    return Dir::Rand;
  if(src=="DIR")
    return Dir::Dir;
  if(src=="TARGET")
    return Dir::Target;
  return Dir::Rand;
  }

ParticleFx::Orientation ParticleFx::loadOrientation(std::string_view src) {
  if(src=="NONE")
    return Orientation::None;
  if(src=="VELO")
    return Orientation::Velocity;
  if(src=="VELO3D")
    return Orientation::Velocity3d;
  return Orientation::None;
  }

float ParticleFx::fetchScaleKey(uint64_t time, const KeyList& keys, float fps, bool smooth, bool loop) const {
  if(keys.size()==0)
    return 1.f;

  uint64_t timeScaled = uint64_t(fps*float(time));
  size_t   at         = size_t(timeScaled/1000);
  float    alpha      = float(timeScaled%1000)/1000.f;

  size_t frameA = 0, frameB = 0;
  if(loop) {
    frameA = (at  )%keys.size();
    frameB = (at+1)%keys.size();
    } else {
    frameA = std::min(at,  keys.size()-1);
    frameB = std::min(at+1,keys.size()-1);
    }
  float k0 = keys[frameA];
  float k1 = keys[frameB];
  if(smooth)
    return k0+alpha*(k1-k0);
  if(alpha<0.5)
    return k0; else
    return k1;
  }
