#include "vobreflection.h"

#include <Tempest/Log>

#include <zenkit/vobs/Misc.hh>
#include <zenkit/vobs/Light.hh>
#include <zenkit/vobs/Trigger.hh>
#include <zenkit/vobs/Camera.hh>
#include <zenkit/vobs/MovableObject.hh>
#include <zenkit/vobs/Sound.hh>
#include <zenkit/vobs/Zone.hh>

#include <algorithm>

using namespace Tempest;

template<class T>
static const T variantCast(const Variant& v) {
  if(auto* r = v.get<T>())
    return *r;
  if constexpr(std::is_same_v<T,zenkit::Color>) {
    if(auto f = v.get<Vec3>()) {
      auto x = std::clamp(f->x, 0.f, 255.f);
      auto y = std::clamp(f->y, 0.f, 255.f);
      auto z = std::clamp(f->z, 0.f, 255.f);
      return zenkit::Color(uint8_t(x), uint8_t(y), uint8_t(z), 255);
      }
    if(auto f = v.get<Vec4>()) {
      auto x = std::clamp(f->x, 0.f, 255.f);
      auto y = std::clamp(f->y, 0.f, 255.f);
      auto z = std::clamp(f->z, 0.f, 255.f);
      auto w = std::clamp(f->w, 0.f, 255.f);
      return zenkit::Color(uint8_t(x), uint8_t(y), uint8_t(z), uint8_t(w));
      }
    }

  if constexpr(std::is_same_v<T,bool>) {
    if(auto f = v.get<int>()) {
      return bool(*f!=0);
      }
    }

  Tempest::Log::d("failed variant cast (", typeid(T).name(), ")");
  return T();
  }

template<class T, class F>
VobReflection::Field& VobReflection::addProperty(std::vector<Field>& fields, std::string_view name, F T::*, std::false_type) {
  Field id;
  id.slot.name       = name;
  id.slot.type       = &Property::Type::Enum;
  id.slot.enumValues = {"TODO"};
  return fields.emplace_back(std::move(id));
  }

template<class T, class F>
VobReflection::Field& VobReflection::addProperty(std::vector<Field>& fields, std::string_view name, F T::* field) {
  Field id;
  id.slot.name = name;
  if constexpr(std::is_same_v<F,bool>) {
    id.slot.type = &Property::Type::Bool1;
    }
  else if constexpr(std::is_same_v<F,float>) {
    id.slot.type = &Property::Type::Vec1;
    }
  else if constexpr(std::is_same_v<F,Vec2>) {
    id.slot.type = &Property::Type::Vec2;
    }
  else if constexpr(std::is_same_v<F,Vec3>) {
    id.slot.type = &Property::Type::Vec3;
    }
  else if constexpr(std::is_same_v<F,zenkit::Vec3>) {
    id.slot.type = &Property::Type::Vec3;
    }
  else if constexpr(std::is_same_v<F,int>) {
    id.slot.type = &Property::Type::Int1;
    }
  else if constexpr(std::is_same_v<F,std::string>) {
    id.slot.type = &Property::Type::String;
    }
  else if constexpr(std::is_same_v<F,zenkit::Color>) {
    id.slot.type = &Property::Type::Color;
    }
  else if constexpr(std::is_same_v<F,std::vector<float>>) {
    id.slot.type = &Property::Type::RangeScale;
    }
  else if constexpr(std::is_same_v<F,std::vector<zenkit::Color>>) {
    id.slot.type = &Property::Type::ColorList;
    }
  else if constexpr(std::is_same_v<F,zenkit::SpriteAlignment>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"None", "Yaw", "Full"};
    }
  else if constexpr(std::is_same_v<F,zenkit::ShadowType>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"None", "Blob"};
    }
  else if constexpr(std::is_same_v<F,zenkit::AnimationType>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"None", "Wind", "Wind2"};
    }
  else if constexpr(std::is_same_v<F,zenkit::MoverMessageType>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"FixedDirect", "FixedOrder", "Next", "Previous"};
    }
  else if constexpr(std::is_same_v<F,zenkit::LightType>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"Point", "Spot"};
    }
  else if constexpr(std::is_same_v<F,zenkit::LightQuality>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"High", "Medium", "Low"};
    }
  else if constexpr(std::is_same_v<F,zenkit::MessageFilterAction>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"None", "Trigger", "Untrigger", "Enable", "Disable", "Toggle"};
    }
  else if constexpr(std::is_same_v<F,zenkit::CameraMotion>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"Undefined", "Smooth", "Linear", "Step", "Slow", "Fast"};
    }
  else if constexpr(std::is_same_v<F,zenkit::TouchCollisionType>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"None", "Box", "Point"};
    }
  else if constexpr(std::is_same_v<F,zenkit::SoundMaterialType>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"Wood", "Stone", "Metal", "Leather", "Clay", "Glass"};
    }
  else if constexpr(std::is_same_v<F,zenkit::TriggerBatchMode>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"All", "Next", "Random"};
    }
  else if constexpr(std::is_same_v<F,zenkit::MoverBehavior>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"Toggle", "Trigger control", "Open timed", "Loop", "Single keys"};
    }
  else if constexpr(std::is_same_v<F,zenkit::MoverLerpType>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"Linear", "Curve"};
    }
  else if constexpr(std::is_same_v<F,zenkit::MoverSpeedType>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"Constant", "Slow start end", "Slow start", "Slow end", "Segment slow start end", "Segment slow start", "Segment slow end"};
    }
  else if constexpr(std::is_same_v<F,zenkit::SoundMode>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"Loop", "Once", "Random"};
    }
  else if constexpr(std::is_same_v<F,zenkit::SoundTriggerVolumeType>) {
    id.slot.type = &Property::Type::Enum;
    id.slot.enumValues = {"Spherical", "Ellipsoidal"};
    }
  else if constexpr(std::is_same_v<F,std::shared_ptr<zenkit::Visual>>) {
    id.slot.type = &Property::Type::Mesh;
    }
  else {
    static_assert(false);
    }

  id.get = [field](const zenkit::VirtualObject& vob) -> Variant {
    if(auto d = dynamic_cast<const T*>(&vob)) {
      return Variant(d->*field);
      }
    return Variant();
    };

  id.exchange = [field](zenkit::VirtualObject& vob, Variant& value) {
    auto* obj = dynamic_cast<T*>(&vob);
    if(obj==nullptr)
      return false;

    auto next = variantCast<F>(value);
    value.set(obj->*field);
    obj->*field = std::move(next);
    return true;
    };
  return fields.emplace_back(std::move(id));
  }

template<class T, class F>
VobReflection::Field& VobReflection::addProperty(std::vector<Field>& fields, std::string_view name, F T::* field, F min, F max) {
  auto& ret = addProperty(fields, name, field);
  ret.slot.min = Tempest::Vec4(min);
  ret.slot.max = Tempest::Vec4(max);
  return ret;
  }

VobReflection::VobReflection() {
  register_zCVob();
  register_zCVobLevelCompo();
  register_oCItem();
  register_oCNpc();
  register_zCMoverController();
  register_zCVobScreenFX();
  register_zCVobStair();
  register_zCPFXController();
  register_zCVobAnimate();
  register_zCVobLensFlare();
  register_zCVobLight();
  register_zCVobSpot();
  register_zCVobStartpoint();
  register_zCMessageFilter();
  register_zCCodeMaster();
  register_zCTriggerWorldStart();
  register_zCCSCamera();
  register_zCCamTrj_KeyFrame();
  register_oCTouchDamage();
  register_zCTriggerUntouch();
  register_zCEarthquake();
  register_oCMOB();
  register_oCMobInter();
  register_oCMobBed();
  register_oCMobFire();
  register_oCMobLadder();
  register_oCMobSwitch();
  register_oCMobWheel();
  register_oCMobContainer();
  register_oCMobDoor();
  register_zCTrigger();
  register_zCTriggerList();
  register_oCTriggerScript();
  register_oCTriggerChangeLevel();
  register_oCCSTrigger();
  register_zCMover();
  register_zCVobSound();
  register_zCVobSoundDaytime();
  register_oCZoneMusic();
  register_oCZoneMusicDefault();
  register_zCZoneZFog();
  register_zCZoneZFogDefault();
  register_zCZoneVobFarPlane();
  register_zCZoneVobFarPlaneDefault();
  }

VobReflection::Class& VobReflection::addClass(Type type, std::string_view name, Type baseType) {
  return classes.try_emplace(type, Class(type, name, baseType)).first->second;
  }

void VobReflection::register_zCVob() {
  auto& cls = addClass(Type::zCVob, "zCVob", Type::UNKNOWN);
  addProperty(cls.fields, "vobName",               &zenkit::VirtualObject::vob_name);
  addProperty(cls.fields, "visual",                &zenkit::VirtualObject::visual);
  addProperty(cls.fields, "showVisual",            &zenkit::VirtualObject::show_visual);
  addProperty(cls.fields, "visualCamAlign",        &zenkit::VirtualObject::sprite_camera_facing_mode);
  addProperty(cls.fields, "visualAniMode",         &zenkit::VirtualObject::anim_mode);
  addProperty(cls.fields, "visualAniModeStrength", &zenkit::VirtualObject::anim_strength);
  addProperty(cls.fields, "vobFarClipZScale",      &zenkit::VirtualObject::far_clip_scale);
  addProperty(cls.fields, "cdStatic",              &zenkit::VirtualObject::cd_static);
  addProperty(cls.fields, "cdDyn",                 &zenkit::VirtualObject::cd_dynamic);
  addProperty(cls.fields, "staticVob",             &zenkit::VirtualObject::vob_static);
  addProperty(cls.fields, "dynShadow",             &zenkit::VirtualObject::dynamic_shadows);
  addProperty(cls.fields, "zbias",                 &zenkit::VirtualObject::bias, 0, 2);
  addProperty(cls.fields, "isAmbient",             &zenkit::VirtualObject::ambient);
  }

void VobReflection::register_zCVobLevelCompo() {
  addClass(Type::zCVobLevelCompo, "zCVobLevelCompo", Type::zCVob);
  }

void VobReflection::register_oCItem() {
  auto& cls = addClass(Type::oCItem, "oCItem", Type::zCVob);
  addProperty(cls.fields, "itemInstance", &zenkit::VItem::instance);
  }

void VobReflection::register_oCNpc() {
  addClass(Type::oCNpc, "oCNpc", Type::zCVob);
  }

void VobReflection::register_zCMoverController() {
  auto& cls = addClass(Type::zCMoverController, "zCMoverController", Type::zCVob);
  addProperty(cls.fields, "target",  &zenkit::VMoverController::target);
  addProperty(cls.fields, "message", &zenkit::VMoverController::message);
  addProperty(cls.fields, "key",     &zenkit::VMoverController::key);
  }

void VobReflection::register_zCVobScreenFX() {
  addClass(Type::zCVobScreenFX, "VScreenEffect", Type::zCVob);
  }

void VobReflection::register_zCVobStair() {
  addClass(Type::zCVobStair, "VStair", Type::zCVob);
  }

void VobReflection::register_zCPFXController() {
  auto& cls = addClass(Type::zCPFXController, "zCPFXController", Type::zCVob);
  addProperty(cls.fields, "pfxName",         &zenkit::VParticleEffectController::pfx_name);
  addProperty(cls.fields, "killVobWhenDone", &zenkit::VParticleEffectController::kill_when_done);
  addProperty(cls.fields, "pfxStartOn",      &zenkit::VParticleEffectController::initially_running);
  }

void VobReflection::register_zCVobAnimate() {
  auto& cls = addClass(Type::zCVobAnimate, "zCVobAnimate", Type::zCVob);
  addProperty(cls.fields, "startOn", &zenkit::VAnimate::start_on);
  }

void VobReflection::register_zCVobLensFlare() {
  auto& cls = addClass(Type::zCVobLensFlare, "zCVobLensFlare", Type::zCVob);
  addProperty(cls.fields, "lensflareFX", &zenkit::VLensFlare::fx);
  }

void VobReflection::register_zCVobLight() {
  auto& cls = addClass(Type::zCVobLight, "zCVobLight", Type::zCVob);
  addProperty(cls.fields, "lightPresetInUse",       &zenkit::VLight::preset);
  addProperty(cls.fields, "lightType",              &zenkit::VLight::light_type);
  addProperty(cls.fields, "lightStatic",            &zenkit::VLight::is_static);
  addProperty(cls.fields, "turnedOn",               &zenkit::VLight::on);
  addProperty(cls.fields, "color",                  &zenkit::VLight::color);
  addProperty(cls.fields, "color animation list",   &zenkit::VLight::color_animation_list);
  addProperty(cls.fields, "color animation smooth", &zenkit::VLight::color_animation_smooth);
  addProperty(cls.fields, "color animation fps",    &zenkit::VLight::color_animation_fps, 0.f, 60.f);
  addProperty(cls.fields, "range",                  &zenkit::VLight::range, 0.f, 2000.f);
  addProperty(cls.fields, "range animation scale",  &zenkit::VLight::range_animation_scale);
  addProperty(cls.fields, "range animation smooth", &zenkit::VLight::range_animation_smooth);
  addProperty(cls.fields, "range animation fps",    &zenkit::VLight::range_animation_fps, 0.f, 60.f);
  addProperty(cls.fields, "spotConeAngle",          &zenkit::VLight::cone_angle, 0.f, 180.f);
  addProperty(cls.fields, "lightQuality",           &zenkit::VLight::quality);
  addProperty(cls.fields, "lensflareFX",            &zenkit::VLight::lensflare_fx);
  addProperty(cls.fields, "can move",               &zenkit::VLight::can_move);
  }

void VobReflection::register_zCVobSpot() {
  addClass(Type::zCVobSpot, "zCVobSpot", Type::zCVob);
  }

void VobReflection::register_zCVobStartpoint() {
  addClass(Type::zCVobStartpoint, "zCVobStartpoint", Type::zCVob);
  }

void VobReflection::register_zCMessageFilter() {
  auto& cls = addClass(Type::zCMessageFilter, "zCMessageFilter", Type::zCVob);
  addProperty(cls.fields, "target",       &zenkit::VMessageFilter::target);
  addProperty(cls.fields, "on trigger",   &zenkit::VMessageFilter::on_trigger);
  addProperty(cls.fields, "on untrigger", &zenkit::VMessageFilter::on_untrigger);
  }

void VobReflection::register_zCCodeMaster() {
  auto& cls = addClass(Type::zCCodeMaster, "zCCodeMaster", Type::zCVob);
  addProperty(cls.fields, "target",                 &zenkit::VCodeMaster::target);
  addProperty(cls.fields, "ordered",                &zenkit::VCodeMaster::ordered);
  addProperty(cls.fields, "first false is failure", &zenkit::VCodeMaster::first_false_is_failure);
  addProperty(cls.fields, "failure target",         &zenkit::VCodeMaster::failure_target);
  addProperty(cls.fields, "untriggered cancels",    &zenkit::VCodeMaster::untriggered_cancels);
  addProperty(cls.fields, "slaves",                 &zenkit::VCodeMaster::slaves, std::false_type());
  }

void VobReflection::register_zCTriggerWorldStart() {
  auto& cls = addClass(Type::zCTriggerWorldStart, "zCTriggerWorldStart", Type::zCVob);
  addProperty(cls.fields, "target",    &zenkit::VTriggerWorldStart::target);
  addProperty(cls.fields, "fire once", &zenkit::VTriggerWorldStart::fire_once);
  }

void VobReflection::register_zCCSCamera() {
  addClass(Type::zCCSCamera, "zCCSCamera", Type::zCVob);
  }

void VobReflection::register_zCCamTrj_KeyFrame() {
  auto& cls = addClass(Type::zCCamTrj_KeyFrame, "zCCamTrj_KeyFrame", Type::zCVob);
  addProperty(cls.fields, "time",                   &zenkit::VCameraTrajectoryFrame::time, 0.f, 6000.f);
  addProperty(cls.fields, "roll angle",             &zenkit::VCameraTrajectoryFrame::roll_angle, -180.f, 180.f);
  addProperty(cls.fields, "fov scale",              &zenkit::VCameraTrajectoryFrame::fov_scale, 0.f, 2.f);
  addProperty(cls.fields, "motion type",            &zenkit::VCameraTrajectoryFrame::motion_type);
  addProperty(cls.fields, "motion type fov",        &zenkit::VCameraTrajectoryFrame::motion_type_fov);
  addProperty(cls.fields, "motion type roll",       &zenkit::VCameraTrajectoryFrame::motion_type_roll);
  addProperty(cls.fields, "motion type time scale", &zenkit::VCameraTrajectoryFrame::motion_type_time_scale);
  addProperty(cls.fields, "tension",                &zenkit::VCameraTrajectoryFrame::tension);
  addProperty(cls.fields, "cam bias",               &zenkit::VCameraTrajectoryFrame::cam_bias);
  addProperty(cls.fields, "continuity",             &zenkit::VCameraTrajectoryFrame::continuity);
  addProperty(cls.fields, "time scale",             &zenkit::VCameraTrajectoryFrame::time_scale);
  addProperty(cls.fields, "time fixed",             &zenkit::VCameraTrajectoryFrame::time_fixed);
  }

void VobReflection::register_oCTouchDamage() {
  auto& cls = addClass(Type::oCTouchDamage, "oCTouchDamage", Type::zCVob);
  addProperty(cls.fields, "damage",           &zenkit::VTouchDamage::damage, 0.f, 1000.f);
  addProperty(cls.fields, "barrier",          &zenkit::VTouchDamage::barrier);
  addProperty(cls.fields, "blunt",            &zenkit::VTouchDamage::blunt);
  addProperty(cls.fields, "edge",             &zenkit::VTouchDamage::edge);
  addProperty(cls.fields, "fire",             &zenkit::VTouchDamage::fire);
  addProperty(cls.fields, "fly",              &zenkit::VTouchDamage::fly);
  addProperty(cls.fields, "magic",            &zenkit::VTouchDamage::magic);
  addProperty(cls.fields, "point",            &zenkit::VTouchDamage::point);
  addProperty(cls.fields, "fall",             &zenkit::VTouchDamage::fall);
  addProperty(cls.fields, "repeat delay sec", &zenkit::VTouchDamage::repeat_delay_sec, 0.f, 10.f);
  addProperty(cls.fields, "volume scale",     &zenkit::VTouchDamage::volume_scale, 0.f, 3.f);
  addProperty(cls.fields, "collision",        &zenkit::VTouchDamage::collision);
  }

void VobReflection::register_zCTriggerUntouch() {
  auto& cls = addClass(Type::zCTriggerUntouch, "zCTriggerUntouch", Type::zCVob);
  addProperty(cls.fields, "target", &zenkit::VTriggerUntouch::target);
  }

void VobReflection::register_zCEarthquake() {
  auto& cls = addClass(Type::zCEarthquake, "zCEarthquake", Type::zCVob);
  addProperty(cls.fields, "radius",    &zenkit::VEarthquake::radius, 0.f, 1000.f);
  addProperty(cls.fields, "duration",  &zenkit::VEarthquake::duration);
  addProperty(cls.fields, "amplitude", &zenkit::VEarthquake::amplitude);
  }

void VobReflection::register_oCMOB() {
  auto& cls = addClass(Type::oCMOB, "oCMOB", Type::zCVob);
  addProperty(cls.fields, "name",             &zenkit::VMovableObject::name);
  addProperty(cls.fields, "hp",               &zenkit::VMovableObject::hp,     0, 1000);
  addProperty(cls.fields, "damage",           &zenkit::VMovableObject::damage, 0, 1000);
  addProperty(cls.fields, "movable",          &zenkit::VMovableObject::movable);
  addProperty(cls.fields, "takable",          &zenkit::VMovableObject::takable);
  addProperty(cls.fields, "focus_override",   &zenkit::VMovableObject::focus_override);
  addProperty(cls.fields, "material",         &zenkit::VMovableObject::material);
  addProperty(cls.fields, "visual_destroyed", &zenkit::VMovableObject::visual_destroyed);
  addProperty(cls.fields, "owner",            &zenkit::VMovableObject::owner);
  addProperty(cls.fields, "owner_guild",      &zenkit::VMovableObject::owner_guild);
  addProperty(cls.fields, "destroyed",        &zenkit::VMovableObject::destroyed);
  }

void VobReflection::register_oCMobInter() {
  auto& cls = addClass(Type::oCMobInter, "oCMobInter", Type::oCMOB);
  addProperty(cls.fields, "state count",              &zenkit::VInteractiveObject::state_count, 0, 20);
  addProperty(cls.fields, "target",                   &zenkit::VInteractiveObject::target);
  addProperty(cls.fields, "item",                     &zenkit::VInteractiveObject::item);
  addProperty(cls.fields, "condition function",       &zenkit::VInteractiveObject::condition_function);
  addProperty(cls.fields, "on state change function", &zenkit::VInteractiveObject::on_state_change_function);
  addProperty(cls.fields, "rewind",                   &zenkit::VInteractiveObject::rewind);
  }

void VobReflection::register_oCMobBed() {
  addClass(Type::oCMobBed, "oCMobBed", Type::oCMobInter);
  }

void VobReflection::register_oCMobFire() {
  auto& cls = addClass(Type::oCMobFire, "oCMobFire", Type::oCMobInter);
  addProperty(cls.fields, "state",                    &zenkit::VFire::state, 0, 10);
  addProperty(cls.fields, "target",                   &zenkit::VFire::target);
  addProperty(cls.fields, "item",                     &zenkit::VFire::item);
  addProperty(cls.fields, "condition_function",       &zenkit::VFire::condition_function);
  addProperty(cls.fields, "on_state_change_function", &zenkit::VFire::on_state_change_function);
  addProperty(cls.fields, "rewind",                   &zenkit::VFire::rewind);
  }

void VobReflection::register_oCMobLadder() {
  addClass(Type::oCMobLadder, "oCMobLadder", Type::oCMobInter);
  }

void VobReflection::register_oCMobSwitch() {
  addClass(Type::oCMobSwitch, "oCMobSwitch", Type::oCMobInter);
  }

void VobReflection::register_oCMobWheel() {
  addClass(Type::oCMobWheel, "oCMobWheel", Type::oCMobInter);
  }

void VobReflection::register_oCMobContainer() {
  auto& cls = addClass(Type::oCMobContainer, "oCMobContainer", Type::oCMobInter);
  addProperty(cls.fields, "locked",      &zenkit::VContainer::locked);
  addProperty(cls.fields, "key",         &zenkit::VContainer::key);
  addProperty(cls.fields, "pick string", &zenkit::VContainer::pick_string);
  addProperty(cls.fields, "contents",    &zenkit::VContainer::contents); //FIXME: list of items
  }

void VobReflection::register_oCMobDoor() {
  auto& cls = addClass(Type::oCMobDoor, "oCMobDoor", Type::oCMobInter);
  addProperty(cls.fields, "locked",      &zenkit::VDoor::locked);
  addProperty(cls.fields, "key",         &zenkit::VDoor::key);
  addProperty(cls.fields, "pick string", &zenkit::VDoor::pick_string);
  }

void VobReflection::register_zCTrigger() {
  auto& cls = addClass(Type::zCTrigger, "zCTrigger", Type::zCVob);
  addProperty(cls.fields, "target",               &zenkit::VTrigger::target);
  addProperty(cls.fields, "start enabled",        &zenkit::VTrigger::start_enabled);
  addProperty(cls.fields, "send untrigger",       &zenkit::VTrigger::send_untrigger);
  addProperty(cls.fields, "react to on trigger",  &zenkit::VTrigger::react_to_on_trigger);
  addProperty(cls.fields, "react to on touch",    &zenkit::VTrigger::react_to_on_touch);
  addProperty(cls.fields, "react to on damage",   &zenkit::VTrigger::react_to_on_damage);
  addProperty(cls.fields, "respond to object",    &zenkit::VTrigger::respond_to_object);
  addProperty(cls.fields, "respond to pc",        &zenkit::VTrigger::respond_to_pc);
  addProperty(cls.fields, "respond to npc",       &zenkit::VTrigger::respond_to_npc);
  addProperty(cls.fields, "vob target",           &zenkit::VTrigger::vob_target);
  addProperty(cls.fields, "max activation count", &zenkit::VTrigger::max_activation_count, -1, 10);
  addProperty(cls.fields, "retrigger delay sec",  &zenkit::VTrigger::retrigger_delay_sec, 0.f, 10.f);
  addProperty(cls.fields, "damage threshold",     &zenkit::VTrigger::damage_threshold, 0.f, 10.f);
  addProperty(cls.fields, "fire delay sec",       &zenkit::VTrigger::fire_delay_sec, 0.f, 10.f);
  }

void VobReflection::register_zCTriggerList() {
  auto& cls = addClass(Type::zCTriggerList, "zCTriggerList", Type::zCTrigger);
  addProperty(cls.fields, "mode",   &zenkit::VTriggerList::mode);
  addProperty(cls.fields, "target", &zenkit::VTriggerList::targets, std::false_type());
  }

void VobReflection::register_oCTriggerScript() {
  auto& cls = addClass(Type::oCTriggerScript, "oCTriggerScript", Type::zCTrigger);
  addProperty(cls.fields, "function", &zenkit::VTriggerScript::function);
  }

void VobReflection::register_oCTriggerChangeLevel() {
  auto& cls = addClass(Type::oCTriggerChangeLevel, "oCTriggerChangeLevel", Type::zCTrigger);
  addProperty(cls.fields, "level name", &zenkit::VTriggerChangeLevel::level_name);
  addProperty(cls.fields, "start vob",  &zenkit::VTriggerChangeLevel::start_vob);
  }

void VobReflection::register_oCCSTrigger() {
  addClass(Type::oCCSTrigger, "oCCSTrigger", Type::zCTrigger);
  }

void VobReflection::register_zCMover() {
  auto& cls = addClass(Type::zCMover, "zCMover", Type::zCTrigger);
  addProperty(cls.fields, "behavior",             &zenkit::VMover::behavior);
  addProperty(cls.fields, "touch blocker damage", &zenkit::VMover::touch_blocker_damage);
  addProperty(cls.fields, "stay open time sec",   &zenkit::VMover::stay_open_time_sec, 0.f, 10.f);
  addProperty(cls.fields, "locked",               &zenkit::VMover::locked);
  addProperty(cls.fields, "auto link",            &zenkit::VMover::auto_link);
  addProperty(cls.fields, "auto rotate",          &zenkit::VMover::auto_rotate);
  addProperty(cls.fields, "speed",                &zenkit::VMover::speed);
  addProperty(cls.fields, "lerp mode",            &zenkit::VMover::lerp_mode);
  addProperty(cls.fields, "speed mode",           &zenkit::VMover::speed_mode);
  addProperty(cls.fields, "keyframes",            &zenkit::VMover::keyframes, std::false_type());
  addProperty(cls.fields, "sfx open start",       &zenkit::VMover::sfx_open_start);
  addProperty(cls.fields, "sfx open end",         &zenkit::VMover::sfx_open_end);
  addProperty(cls.fields, "sfx transitioning",    &zenkit::VMover::sfx_transitioning);
  addProperty(cls.fields, "sfx close start",      &zenkit::VMover::sfx_close_start);
  addProperty(cls.fields, "sfx close end",        &zenkit::VMover::sfx_close_end);
  addProperty(cls.fields, "sfx lock",             &zenkit::VMover::sfx_lock);
  addProperty(cls.fields, "sfx unlock",           &zenkit::VMover::sfx_unlock);
  addProperty(cls.fields, "sfx use locked",       &zenkit::VMover::sfx_use_locked);
  }

void VobReflection::register_zCVobSound() {
  auto& cls = addClass(Type::zCVobSound, "zCVobSound", Type::zCVob);
  addProperty(cls.fields, "volume",                 &zenkit::VSound::volume, 0.f, 10.f);
  addProperty(cls.fields, "mode",                   &zenkit::VSound::mode);
  addProperty(cls.fields, "random delay",           &zenkit::VSound::random_delay,     0.f, 10.f);
  addProperty(cls.fields, "random delay variation", &zenkit::VSound::random_delay_var, 0.f, 10.f);
  addProperty(cls.fields, "initially playing",      &zenkit::VSound::initially_playing);
  addProperty(cls.fields, "ambient3d",              &zenkit::VSound::ambient3d);
  addProperty(cls.fields, "obstruction",            &zenkit::VSound::obstruction);
  addProperty(cls.fields, "cone angle",             &zenkit::VSound::cone_angle);
  addProperty(cls.fields, "volume type",            &zenkit::VSound::volume_type);
  addProperty(cls.fields, "radius",                 &zenkit::VSound::radius);
  addProperty(cls.fields, "sound name",             &zenkit::VSound::sound_name);
  }

void VobReflection::register_zCVobSoundDaytime() {
  auto& cls = addClass(Type::zCVobSoundDaytime, "zCVobSoundDaytime", Type::zCVobSound);
  addProperty(cls.fields, "start_time",  &zenkit::VSoundDaytime::start_time, 0.f, 24.f);
  addProperty(cls.fields, "end_time",    &zenkit::VSoundDaytime::end_time,   0.f, 24.f);
  addProperty(cls.fields, "sound_name2", &zenkit::VSoundDaytime::sound_name2);
  }

void VobReflection::register_oCZoneMusic() {
  auto& cls = addClass(Type::oCZoneMusic, "oCZoneMusic", Type::zCVob);
  addProperty(cls.fields, "enabled",   &zenkit::VZoneMusic::enabled);
  addProperty(cls.fields, "priority",  &zenkit::VZoneMusic::priority);
  addProperty(cls.fields, "ellipsoid", &zenkit::VZoneMusic::ellipsoid);
  addProperty(cls.fields, "reverb",    &zenkit::VZoneMusic::reverb, 0.f, 2.f);
  addProperty(cls.fields, "volume",    &zenkit::VZoneMusic::volume, 0.f, 2.f);
  addProperty(cls.fields, "loop",      &zenkit::VZoneMusic::loop);
  }

void VobReflection::register_oCZoneMusicDefault() {
  addClass(Type::oCZoneMusicDefault, "oCZoneMusicDefault", Type::oCZoneMusic);
  }

void VobReflection::register_zCZoneZFog() {
  auto& cls = addClass(Type::zCZoneZFog, "zCZoneZFog", Type::zCVob);
  addProperty(cls.fields, "range center",           &zenkit::VZoneFog::range_center, 0.f, 1000.f);
  addProperty(cls.fields, "inner range percentage", &zenkit::VZoneFog::inner_range_percentage, 0.f, 100.f);
  addProperty(cls.fields, "color",                  &zenkit::VZoneFog::color);
  addProperty(cls.fields, "fade out sky",           &zenkit::VZoneFog::fade_out_sky);
  addProperty(cls.fields, "override color",         &zenkit::VZoneFog::override_color);
  }

void VobReflection::register_zCZoneZFogDefault() {
  addClass(Type::zCZoneZFogDefault, "zCZoneZFogDefault", Type::zCZoneZFog);
  }

void VobReflection::register_zCZoneVobFarPlane() {
  auto& cls = addClass(Type::zCZoneVobFarPlane, "zCZoneVobFarPlane", Type::zCVob);
  addProperty(cls.fields, "vob far plane z",        &zenkit::VZoneFarPlane::vob_far_plane_z,        0.f, 1.f);
  addProperty(cls.fields, "inner range percentage", &zenkit::VZoneFarPlane::inner_range_percentage, 0.f, 100.f);
  }

void VobReflection::register_zCZoneVobFarPlaneDefault() {
  addClass(Type::zCZoneVobFarPlaneDefault, "zCZoneVobFarPlaneDefault", Type::zCZoneVobFarPlane);
  }

const VobReflection& VobReflection::instance() {
  static const VobReflection reflection;
  return reflection;
  }

const VobReflection::Class* VobReflection::find(Type type) {
  const auto& classes = instance().classes;
  const auto it = classes.find(type);
  return it==classes.end() ? nullptr : &it->second;
  }

const VobReflection::Class* VobReflection::Class::baseClass() const {
  return find(baseType);
  }

std::span<const VobReflection::Field> VobReflection::Class::properties() const {
  return fields;
  }

bool VobReflection::Class::isDerivedOf(Type base) const {
  for(auto cls = baseClass(); cls!=nullptr; cls = cls->baseClass())
    if(cls->type==base)
      return true;
  return false;
  }

bool VobReflection::Class::isBaseOf(Type derived) const {
  const auto* cls = find(derived);
  return cls!=nullptr && cls->isDerivedOf(type);
  }
