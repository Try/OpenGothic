#include "visualfx.h"

#include <Tempest/Log>
#include <cctype>

#include "utils/string_frm.h"
#include "world/world.h"
#include "utils/parser.h"
#include "gothic.h"

using namespace Tempest;

VisualFx::Key::Key(const phoenix::c_particle_fx_emit_key& k) {
  visName                 = Gothic::inst().loadParticleFx(k.vis_name_s);
  visSizeScale            = k.vis_size_scale;
  scaleDuration           = k.scale_duration; // time to reach full scale at this key for relevant vars (size, alpha, etc.)

  pfx_ppsValue            = k.pfx_pps_value;
  pfx_ppsIsSmoothChg      = k.pfx_pps_is_smooth_chg!=0;
  pfx_ppsIsLoopingChg     = k.pfx_pps_is_looping_chg!=0;
  pfx_scTime              = 0.f;
  if(!k.pfx_fly_gravity_s.empty())
    pfx_flyGravity = Parser::loadVec3(k.pfx_fly_gravity_s);

  if(!k.pfx_shp_dim_s.empty())
    pfx_shpDim = Parser::loadVec3(k.pfx_shp_dim_s);
  pfx_shpIsVolumeChg      = k.pfx_shp_is_volume_chg;    // changes volume rendering of pfx if set to 1
  pfx_shpScaleFPS         = k.pfx_shp_scale_fps;
  pfx_shpDistribWalkSpeed = k.pfx_shp_distrib_walks_peed;
  if(!k.pfx_shp_offset_vec_s.empty())
    pfx_shpOffsetVec = Parser::loadVec3(k.pfx_shp_offset_vec_s);
  pfx_shpDistribType_S    = k.pfx_shp_distrib_type_s;
  pfx_dirMode_S           = k.pfx_dir_mode_s;
  pfx_dirFOR_S            = k.pfx_dir_for_s;
  pfx_dirModeTargetFOR_S  = k.pfx_dir_mode_target_for_s;
  pfx_dirModeTargetPos_S  = k.pfx_dir_mode_target_pos_s;
  pfx_velAvg              = k.pfx_vel_avg;
  pfx_lspPartAvg          = k.pfx_lsp_part_avg;
  pfx_visAlphaStart       = k.pfx_vis_alpha_start;

  lightPresetName         = k.light_preset_name;
  lightRange              = k.light_range;
  sfxID                   = k.sfx_id;
  sfxIsAmbient            = k.sfx_is_ambient;
  emCreateFXID            = Gothic::inst().loadVisualFx(k.em_create_fx_id);

  emFlyGravity            = k.em_fly_gravity;
  if(!k.em_self_rot_vel_s.empty())
    emSelfRotVel          = Parser::loadVec3(k.em_self_rot_vel_s);
  if(!k.em_trj_mode_s.empty())
    emTrjMode             = VisualFx::loadTrajectory(k.em_trj_mode_s);
  emTrjEaseVel            = k.em_trj_ease_vel;
  emCheckCollision        = k.em_check_collision!=0;
  emFXLifeSpan            = k.em_fx_lifespan<0 ? 0 : uint64_t(k.em_fx_lifespan*1000.f);
  }

VisualFx::VisualFx(const phoenix::c_fx_base& fx, phoenix::vm& vm, std::string_view name) {
  visName_S                = fx.vis_name_s;
  visSize                  = Parser::loadVec2(fx.vis_size_s);
  visAlpha                 = fx.vis_alpha;
  visAlphaBlendFunc        = Parser::loadAlpha(fx.vis_alpha_blend_func_s);
  visTexAniFPS             = 0.f;
  visTexAniIsLooping       = false;

  emTrjMode                = loadTrajectory(fx.em_trj_mode_s);
  emTrjOriginNode          = fx.em_trj_origin_node;
  emTrjTargetNode          = fx.em_trj_target_node;
  emTrjTargetRange         = fx.em_trj_target_range;
  emTrjTargetAzi           = fx.em_trj_target_azi;
  emTrjTargetElev          = fx.em_trj_target_elev;
  emTrjNumKeys             = fx.em_trj_num_keys;
  emTrjNumKeysVar          = fx.em_trj_num_keys_var;
  emTrjAngleElevVar        = fx.em_trj_angle_elev_var;
  emTrjAngleHeadVar        = fx.em_trj_angle_head_var;
  emTrjKeyDistVar          = fx.em_trj_key_dist_var;
  emTrjLoopMode            = loadLoopmode(fx.em_trj_loop_mode_s);
  emTrjEaseFunc            = loadEaseFunc(fx.em_trj_ease_func_s);
  emTrjEaseVel             = fx.em_trj_ease_vel;
  emTrjDynUpdateDelay      = fx.em_trj_dyn_update_delay;
  emTrjDynUpdateTargetOnly = fx.em_trj_dyn_update_target_only!=0;

  emFXCreate               = Gothic::inst().loadVisualFx(fx.em_fx_create_s);
  emFXInvestOrigin         = fx.em_fx_invest_origin_s;
  emFXInvestTarget         = fx.em_fx_invest_target_s;
  emFXTriggerDelay         = fx.em_fx_trigger_delay<0 ? 0 : uint64_t(fx.em_fx_trigger_delay*1000.f);
  emFXCreatedOwnTrj        = fx.em_fx_create_down_trj!=0;
  emActionCollDyn          = strToColision(fx.em_action_coll_dyn_s);		// CREATE, BOUNCE, CREATEONCE, NORESP, COLLIDE
  emActionCollStat         = strToColision(fx.em_action_coll_stat_s);			// CREATE, BOUNCE, CREATEONCE, NORESP, COLLIDE, CREATEQUAD
  emFXCollStat             = Gothic::inst().loadVisualFx(fx.em_fx_coll_stat_s);
  emFXCollDyn              = Gothic::inst().loadVisualFx(fx.em_fx_coll_dyn_s);
  emFXCollDynPerc          = Gothic::inst().loadVisualFx(fx.em_fx_coll_dyn_perc_s);
  emFXCollStatAlign        = loadCollisionAlign(fx.em_fx_coll_stat_align_s);
  emFXCollDynAlign         = loadCollisionAlign(fx.em_fx_coll_dyn_align_s);
  emFXLifeSpan             = fx.em_fx_lifespan<0 ? 0 : uint64_t(fx.em_fx_lifespan*1000.f);

  emCheckCollision         = fx.em_check_collision;
  emAdjustShpToOrigin      = fx.em_adjust_shp_to_origin;
  emInvestNextKeyDuration  = fx.em_invest_next_key_duration<0 ? 0 : uint64_t(fx.em_invest_next_key_duration*1000.f);
  emFlyGravity             = fx.em_fly_gravity;
  emSelfRotVel             = Parser::loadVec3(fx.em_self_rot_vel_s);
  for(size_t i=0; i<phoenix::c_fx_base::user_string_count; ++i)
    userString[i] = fx.user_string[i];
  lightPresetName          = fx.light_preset_name;
  sfxID                    = fx.sfx_id;
  sfxIsAmbient             = fx.sfx_is_ambient;
  sendAssessMagic          = fx.send_assess_magic;
  secsPerDamage            = fx.secs_per_damage;

  for(auto& c:emTrjOriginNode)
    c = char(std::toupper(c));

  static const char* keyName[int(SpellFxKey::Count)] = {
    "OPEN",
    "INIT",
    "CAST",
    "INVEST",
    "COLLIDE"
    };

  for(int i=0; i<int(SpellFxKey::Count); ++i) {
    string_frm kname(name,"_KEY_",keyName[i]);
    auto id = vm.find_symbol_by_name(kname);
    if(id==nullptr)
      continue;
    auto key = vm.init_instance<phoenix::c_particle_fx_emit_key>(id);
    keys  [i] = Key(*key);
    hasKey[i] = true;
    }

  for(int i=1; ; ++i) {
    string_frm kname(name,"_KEY_INVEST_",i);
    auto id = vm.find_symbol_by_name(kname);
    if(id==nullptr)
      break;
    auto key = vm.init_instance<phoenix::c_particle_fx_emit_key>(id);
    // keys[int(SpellFxKey::Invest)] = key;
    investKeys.emplace_back(*key);
    }
  }

uint64_t VisualFx::effectPrefferedTime() const {
  return emFXLifeSpan;
  }

PfxEmitter VisualFx::visual(World& owner) const {
  return PfxEmitter(owner,visName_S);
  }

const VisualFx::Key* VisualFx::key(SpellFxKey type, int32_t keyLvl) const {
  if(type==SpellFxKey::Count)
    return nullptr;

  if(type==SpellFxKey::Invest && keyLvl>0) {
    keyLvl--;
    if(size_t(keyLvl)<investKeys.size())
      return &investKeys[size_t(keyLvl)];
    return nullptr;
    }

  if(!hasKey[int(type)])
    return nullptr;
  return &keys[int(type)];
  }

VisualFx::Trajectory VisualFx::loadTrajectory(std::string_view str) {
  uint8_t bits = 0;
  size_t  prev = 0;
  auto    s    = str.data();
  for(size_t i=0; i<str.size(); ++i) {
    if(s[i]==' ' || s[i]=='\0') {
      if(std::memcmp(s+prev,"NONE",i-prev)==0) {
        bits |= 0; // no effect
        }
      else if(std::memcmp(s+prev,"TARGET",i-prev)==0) {
        bits |= Trajectory::Target;
        }
      else if(std::memcmp(s+prev,"LINE",i-prev)==0) {
        bits |= Trajectory::Line;
        }
      else if(std::memcmp(s+prev,"SPLINE",i-prev)==0) {
        bits |= Trajectory::Spline;
        }
      else if(std::memcmp(s+prev,"RANDOM",i-prev)==0) {
        bits |= Trajectory::Random;
        }
      else if(std::memcmp(s+prev,"FIXED",i-prev)==0) {
        bits |= Trajectory::Fixed;
        }
      else if(std::memcmp(s+prev,"FOLLOW",i-prev)==0) {
        bits |= Trajectory::Follow;
        }
      else {
        if(i!=prev)
          Log::d("unknown trajectory flag: \"",s+prev,"\"");
        }
      prev = i+1;
      }
    }
  return Trajectory(bits);
  }

VisualFx::LoopMode VisualFx::loadLoopmode(std::string_view str) {
  if(str=="NONE")
    return LoopMode::LoopModeNone;
  if(str=="PINGPONG")
    return LoopMode::PinPong;
  if(str=="PINGPONG_ONCE")
    return LoopMode::PinPongOnce;
  if(str=="HALT")
    return LoopMode::Halt;
  return LoopMode::LoopModeNone;
  }

VisualFx::EaseFunc VisualFx::loadEaseFunc(std::string_view str) {
  if(str=="LINEAR")
    return EaseFunc::Linear;
  return EaseFunc::Linear;
  }

VisualFx::CollisionAlign VisualFx::loadCollisionAlign(std::string_view str) {
  if(str=="COLLISIONNORMAL")
    return CollisionAlign::Normal;
  if(str=="TRAJECTORY")
    return CollisionAlign::Trajectory;
  return CollisionAlign::Normal;
  }

VisualFx::Collision VisualFx::strToColision(std::string_view sv) {
  uint8_t bits = 0;
  while(sv.size()>0) {
    auto sp   = sv.find(' ');
    auto word = sv.substr(0,sp);
    if(word=="COLLIDE") {
      bits |= Collision::Collide;
      }
    else if(word=="CREATE") {
      bits |= Collision::Create;
      }
    else if(word=="CREATEONCE") {
      bits |= Collision::CreateOnce;
      }
    else if(word=="NORESP") {
      bits |= Collision::NoResp;
      }
    else if(word=="NORESP") {
      bits |= Collision::NoResp;
      }
    else if(word=="CREATEQUAD") {
      bits |= Collision::CreateQuad;
      }
    else {
      Log::d("unknown collision flag: \"",word,"\"");
      }
    if(sp==std::string_view::npos)
      break;
    sv = sv.substr(sp+1);
    }
  return Collision(bits);
  }
