#include "propertydelegate.h"

#include <Tempest/Log>

#include <zenkit/vobs/Misc.hh>
#include <zenkit/vobs/Light.hh>
#include <zenkit/vobs/Trigger.hh>
#include <zenkit/vobs/Camera.hh>
#include <zenkit/vobs/MovableObject.hh>
#include <zenkit/vobs/Sound.hh>
#include <zenkit/vobs/Zone.hh>

#include "ui/controls/parameterwidget.h"
#include "ui/controls/widgetheader.h"
#include "ui/property/property.h"

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

struct PropertyDelegate::TypeHeader : WidgetHeader {
  TypeHeader(zenkit::VirtualObjectType c):cat(c){}

  void emitClick() override {
    onChanged(cat);
    }

  Tempest::Signal<void(zenkit::VirtualObjectType)> onChanged;
  zenkit::VirtualObjectType                        cat = {};
  };

PropertyDelegate::PropertyDelegate() {
  }

void PropertyDelegate::setVob(WorldEdit::Vob* inVob) {
  vob = inVob;
  update();
  }

void PropertyDelegate::update() {
  index.clear();
  if(vob!=nullptr)
    mkIndex(vob->get());
  invalidateView();
  }

size_t PropertyDelegate::size() const {
  return index.size();
  }

void PropertyDelegate::onProperty(size_t id, const Variant& v, bool commit) {
  index[id].set(vob, v, commit);
  }

void PropertyDelegate::onCategory(zenkit::VirtualObjectType cat) {
  if(isClosed(cat))
    closed.erase(cat); else
    closed.insert(cat);
  update();
  }

Widget* PropertyDelegate::createView(size_t i) {
  if(index[i].slt.type==nullptr) {
    auto ret = new TypeHeader(index[i].cat);
    ret->setSizePolicy(Preferred,Fixed);
    ret->setText(index[i].slt.name);
    ret->setClosed(isClosed(index[i].cat));
    ret->onChanged.bind(this, &PropertyDelegate::onCategory);
    return ret;
    }
  auto var = index[i].get ? index[i].get(vob) : Variant();
  auto ed  = ParameterWidget::createEditor(index[i].slt, var, i);
  ed->onChanged.bind(this, &PropertyDelegate::onProperty);
  return ed;
  }

bool PropertyDelegate::addHeader(std::string_view name, zenkit::VirtualObjectType type) {
  Index id;
  id.slt.name = name;
  id.slt.type = nullptr;
  id.cat      = type;
  index.push_back(id);
  return isClosed(type);
  }

bool PropertyDelegate::isClosed(zenkit::VirtualObjectType type) const {
  return closed.find(type)!=closed.end();
  }

template<class T, class F>
auto PropertyDelegate::addView(std::string_view name, F T::*, std::false_type) -> PropertyDelegate::Index& {
  Index id;
  id.slt.name       = name;
  id.slt.type       = &Property::Type::Enum;
  id.slt.enumValues = {"TODO"};
  index.push_back(id);
  return index.back();
  }

template<class T, class F>
auto PropertyDelegate::addView(std::string_view name, F T::* field) -> Index& {
  Index id;
  id.slt.name = name;
  if constexpr(std::is_same_v<F,bool>) {
    id.slt.type = &Property::Type::Bool1;
    }
  else if constexpr(std::is_same_v<F,float>) {
    id.slt.type = &Property::Type::Vec1;
    }
  else if constexpr(std::is_same_v<F,Vec2>) {
    id.slt.type = &Property::Type::Vec2;
    }
  else if constexpr(std::is_same_v<F,Vec3>) {
    id.slt.type = &Property::Type::Vec3;
    }
  else if constexpr(std::is_same_v<F,zenkit::Vec3>) {
    id.slt.type = &Property::Type::Vec3;
    }
  else if constexpr(std::is_same_v<F,int>) {
    id.slt.type = &Property::Type::Int1;
    }
  else if constexpr(std::is_same_v<F,std::string>) {
    id.slt.type = &Property::Type::String;
    }
  else if constexpr(std::is_same_v<F,zenkit::Color>) {
    id.slt.type = &Property::Type::Color;
    }
  else if constexpr(std::is_same_v<F,zenkit::SpriteAlignment>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"None", "Yaw", "Full"};
    }
  else if constexpr(std::is_same_v<F,zenkit::ShadowType>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"None", "Blob"};
    }
  else if constexpr(std::is_same_v<F,zenkit::AnimationType>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"None", "Wind", "Wind2"};
    }
  else if constexpr(std::is_same_v<F,zenkit::MoverMessageType>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"FixedDirect", "FixedOrder", "Next", "Previous"};
    }
  else if constexpr(std::is_same_v<F,zenkit::LightType>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"Point", "Spot"};
    }
  else if constexpr(std::is_same_v<F,zenkit::LightQuality>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"High", "Medium", "Low"};
    }
  else if constexpr(std::is_same_v<F,zenkit::MessageFilterAction>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"None", "Trigger", "Untrigger", "Enable", "Disable", "Toggle"};
    }
  else if constexpr(std::is_same_v<F,zenkit::CameraMotion>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"Undefined", "Smooth", "Linear", "Step", "Slow", "Fast"};
    }
  else if constexpr(std::is_same_v<F,zenkit::TouchCollisionType>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"None", "Box", "Point"};
    }
  else if constexpr(std::is_same_v<F,zenkit::SoundMaterialType>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"Wood", "Stone", "Metal", "Leather", "Clay", "Glass"};
    }
  else if constexpr(std::is_same_v<F,zenkit::TriggerBatchMode>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"All", "Next", "Random"};
    }
  else if constexpr(std::is_same_v<F,zenkit::MoverBehavior>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"Toggle", "Trigger control", "Open timed", "Loop", "Single keys"};
    }
  else if constexpr(std::is_same_v<F,zenkit::MoverLerpType>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"Linear", "Curve"};
    }
  else if constexpr(std::is_same_v<F,zenkit::MoverSpeedType>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"Constant", "Slow start end", "Slow start", "Slow end", "Segment slow start end", "Segment slow start", "Segment slow end"};
    }
  else if constexpr(std::is_same_v<F,zenkit::SoundMode>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"Loop", "Once", "Random"};
    }
  else if constexpr(std::is_same_v<F,zenkit::SoundTriggerVolumeType>) {
    id.slt.type = &Property::Type::Enum;
    id.slt.enumValues = {"Spherical", "Ellipsoidal"};
    }
  else {
    static_assert(false);
    }
  /*
  else if constexpr(std::is_same_v<F,std::vector<float>>) {
    //TODO
    id.slt.type = &Property::Type::Vec1;
    }
  else if constexpr(std::is_same_v<F,std::vector<std::string>>) {
    //TODO
    id.slt.type = &Property::Type::Vec1;
    }
  */

  id.get = [field](const WorldEdit::Vob* vob) -> Variant {
    if(auto d = dynamic_cast<const T*>(vob->get())) {
      auto val = (*d.*field);
      return Variant(val);
      }
    return Variant();
    };

  id.set = [field, this](WorldEdit::Vob* vob, const Variant& v, bool commit) {
    auto f = variantCast<F>(v);
    std::unique_ptr<Command::Action<WorldEdit>> ptr(new CmdSetProperty<T, F>(vob, field, f));
    onChanged(ptr, commit);
    };
  index.push_back(id);
  return index.back();
  }

template<class T, class F>
auto PropertyDelegate::addView(std::string_view name, F T::* field, F min, F max) -> PropertyDelegate::Index& {
  auto& ret = addView(name, field);
  ret.slt.min = Tempest::Vec4(min);
  ret.slt.max = Tempest::Vec4(max);
  return ret;
  }

void PropertyDelegate::mkIndex(const zenkit::VirtualObject* vob) {
  index.clear();
  if(vob!=nullptr)
    mkIndex(vob->type, *vob);
  }

void PropertyDelegate::mkIndex(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  switch (type) {
    case zenkit::VirtualObjectType::UNKNOWN:
    case zenkit::VirtualObjectType::zCVob:
      mkIndex_zCVob(type, vob);
      break;
    case zenkit::VirtualObjectType::zCVobLevelCompo:
      mkIndex_zCVobLevelCompo(type, vob);
      break;
    case zenkit::VirtualObjectType::oCItem:
      mkIndex_oCItem(type, vob);
      break;
    case zenkit::VirtualObjectType::oCNpc:
      break;
    case zenkit::VirtualObjectType::zCMoverController:
      mkIndex_zCMoverController(type, vob);
      break;
    case zenkit::VirtualObjectType::zCVobScreenFX:
      mkIndex_zCVobScreenFX(type, vob);
      break;
    case zenkit::VirtualObjectType::zCVobStair:
      mkIndex_zCVobStair(type, vob);
      break;
    case zenkit::VirtualObjectType::zCPFXController:
      mkIndex_zCPFXController(type, vob);
      break;
    case zenkit::VirtualObjectType::zCVobAnimate:
      mkIndex_zCVobAnimate(type, vob);
      break;
    case zenkit::VirtualObjectType::zCVobLensFlare:
      mkIndex_zCVobLensFlare(type, vob);
      break;
    case zenkit::VirtualObjectType::zCVobLight:
      mkIndex_zCVobLight(type, vob);
      break;
    case zenkit::VirtualObjectType::zCVobSpot:
      mkIndex_zCVobSpot(type, vob);
      break;
    case zenkit::VirtualObjectType::zCVobStartpoint:
      mkIndex_zCVobStartpoint(type, vob);
      break;
    case zenkit::VirtualObjectType::zCMessageFilter:
      mkIndex_zCMessageFilter(type, vob);
      break;
    case zenkit::VirtualObjectType::zCCodeMaster:
      mkIndex_zCCodeMaster(type, vob);
      break;
    case zenkit::VirtualObjectType::zCTriggerWorldStart:
      mkIndex_zCTriggerWorldStart(type, vob);
      break;
    case zenkit::VirtualObjectType::zCCSCamera:
      mkIndex_zCCSCamera(type, vob);
      break;
    case zenkit::VirtualObjectType::zCCamTrj_KeyFrame:
      mkIndex_zCCamTrj_KeyFrame(type, vob);
      break;
    case zenkit::VirtualObjectType::oCTouchDamage:
      mkIndex_oCTouchDamage(type, vob);
      break;
    case zenkit::VirtualObjectType::zCTriggerUntouch:
      mkIndex_zCTriggerUntouch(type, vob);
      break;
    case zenkit::VirtualObjectType::zCEarthquake:
      mkIndex_zCEarthquake(type, vob);
      break;
    case zenkit::VirtualObjectType::oCMOB:
      mkIndex_oCMOB(type, vob);
      break;
    case zenkit::VirtualObjectType::oCMobInter:
      mkIndex_oCMobInter(type, vob);
      break;
    case zenkit::VirtualObjectType::oCMobBed:
      mkIndex_oCMobBed(type, vob);
      break;
    case zenkit::VirtualObjectType::oCMobFire:
      mkIndex_oCMobFire(type, vob);
      break;
    case zenkit::VirtualObjectType::oCMobLadder:
      mkIndex_oCMobLadder(type, vob);
      break;
    case zenkit::VirtualObjectType::oCMobSwitch:
      mkIndex_oCMobSwitch(type, vob);
      break;
    case zenkit::VirtualObjectType::oCMobWheel:
      mkIndex_oCMobWheel(type, vob);
      break;
    case zenkit::VirtualObjectType::oCMobContainer:
      mkIndex_oCMobContainer(type, vob);
      break;
    case zenkit::VirtualObjectType::oCMobDoor:
      mkIndex_oCMobDoor(type, vob);
      break;
    case zenkit::VirtualObjectType::zCTrigger:
      mkIndex_zCTrigger(type, vob);
      break;
    case zenkit::VirtualObjectType::zCTriggerList:
      mkIndex_zCTriggerList(type, vob);
      break;
    case zenkit::VirtualObjectType::oCTriggerScript:
      mkIndex_oCTriggerScript(type, vob);
      break;
    case zenkit::VirtualObjectType::oCTriggerChangeLevel:
      mkIndex_oCTriggerChangeLevel(type, vob);
      break;
    case zenkit::VirtualObjectType::oCCSTrigger:
      mkIndex_oCCSTrigger(type, vob);
      break;
    case zenkit::VirtualObjectType::zCMover:
      mkIndex_zCMover(type, vob);
      break;
    case zenkit::VirtualObjectType::zCVobSound:
      mkIndex_zCVobSound(type, vob);
      break;
    case zenkit::VirtualObjectType::zCVobSoundDaytime:
      mkIndex_zCVobSoundDaytime(type, vob);
      break;
    case zenkit::VirtualObjectType::oCZoneMusic:
      mkIndex_oCZoneMusic(type, vob);
      break;
    case zenkit::VirtualObjectType::oCZoneMusicDefault:
      mkIndex_oCZoneMusicDefault(type, vob);
      break;
    case zenkit::VirtualObjectType::zCZoneZFog:
      mkIndex_zCZoneZFog(type, vob);
      break;
    case zenkit::VirtualObjectType::zCZoneZFogDefault:
      mkIndex_zCZoneZFogDefault(type, vob);
      break;
    case zenkit::VirtualObjectType::zCZoneVobFarPlane:
      mkIndex_zCZoneVobFarPlane(type, vob);
      break;
    case zenkit::VirtualObjectType::zCZoneVobFarPlaneDefault:
      mkIndex_zCZoneVobFarPlaneDefault(type, vob);
      break;
    default:
      Log::e("unknown mob type");
    }
  }

void PropertyDelegate::mkIndex_zCVob(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  if(addHeader("zCVob", zenkit::VirtualObjectType::zCVob))
    return;
  addView("vobName",               &zenkit::VirtualObject::vob_name);
  addView("visual",                &zenkit::VirtualObject::visual_name);
  addView("showVisual",            &zenkit::VirtualObject::show_visual);
  addView("visualCamAlign",        &zenkit::VirtualObject::sprite_camera_facing_mode);
  addView("visualAniMode",         &zenkit::VirtualObject::anim_mode);
  addView("visualAniModeStrength", &zenkit::VirtualObject::anim_strength);
  addView("vobFarClipZScale",      &zenkit::VirtualObject::far_clip_scale);
  addView("cdStatic",              &zenkit::VirtualObject::cd_static);
  addView("cdDyn",                 &zenkit::VirtualObject::cd_dynamic);
  addView("staticVob",             &zenkit::VirtualObject::vob_static);
  addView("dynShadow",             &zenkit::VirtualObject::dynamic_shadows);
  addView("zbias",                 &zenkit::VirtualObject::bias, 0, 2);
  addView("isAmbient",             &zenkit::VirtualObject::ambient);
  }

void PropertyDelegate::mkIndex_zCVobLevelCompo(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("zCVobLevelCompo", type);
  }

void PropertyDelegate::mkIndex_oCItem(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("oCItem", zenkit::VirtualObjectType::oCItem))
    return;
  addView("itemInstance", &zenkit::VItem::instance);
  }

void PropertyDelegate::mkIndex_zCMoverController(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCMoverController", zenkit::VirtualObjectType::zCMoverController))
    return;
  addView("target",  &zenkit::VMoverController::target);
  addView("message", &zenkit::VMoverController::message);
  addView("key",     &zenkit::VMoverController::key);
  }

void PropertyDelegate::mkIndex_zCVobScreenFX(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("VScreenEffect", type);
  }

void PropertyDelegate::mkIndex_zCVobStair(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("VStair", type);
  }

void PropertyDelegate::mkIndex_zCPFXController(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCPFXController", zenkit::VirtualObjectType::zCPFXController))
    return;
  addView("pfxName",         &zenkit::VParticleEffectController::pfx_name);
  addView("killVobWhenDone", &zenkit::VParticleEffectController::kill_when_done);
  addView("pfxStartOn",      &zenkit::VParticleEffectController::initially_running);
  }

void PropertyDelegate::mkIndex_zCEffect(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("zCEffect", type);
  }

void PropertyDelegate::mkIndex_zCVobAnimate(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCEffect(type, vob);
  addHeader("zCVobAnimate", zenkit::VirtualObjectType::zCVobAnimate);
  if(isClosed(type))
    return;
  addView("startOn", &zenkit::VAnimate::start_on);
  }

void PropertyDelegate::mkIndex_zCVobLensFlare(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCEffect(type, vob);
  if(addHeader("zCVobLensFlare", zenkit::VirtualObjectType::zCVobLensFlare))
    return;
  addView("lensflareFX", &zenkit::VLensFlare::fx);
  }

void PropertyDelegate::mkIndex_zCVobLight(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCVobLight", zenkit::VirtualObjectType::zCVobLight))
    return;
  addView("lightPresetInUse",       &zenkit::VLight::preset);
  addView("lightType",              &zenkit::VLight::light_type);
  addView("range",                  &zenkit::VLight::range, 0.f, 2000.f);
  addView("color",                  &zenkit::VLight::color);
  addView("spotConeAngle",          &zenkit::VLight::cone_angle, 0.f, 180.f);
  addView("lightStatic",            &zenkit::VLight::is_static);
  addView("lightQuality",           &zenkit::VLight::quality);
  addView("lensflareFX",            &zenkit::VLight::lensflare_fx);
  addView("turnedOn",               &zenkit::VLight::on);
  addView("range animation scale",  &zenkit::VLight::range_animation_scale, std::false_type());
  addView("range animation fps",    &zenkit::VLight::range_animation_fps, 0.f, 60.f);
  addView("range animation smooth", &zenkit::VLight::range_animation_smooth);
  addView("color animation list",   &zenkit::VLight::color_animation_list, std::false_type());
  addView("color animation fps",    &zenkit::VLight::color_animation_fps, 0.f, 60.f);
  addView("color animation smooth", &zenkit::VLight::color_animation_smooth);
  addView("can move",               &zenkit::VLight::can_move);
  }

void PropertyDelegate::mkIndex_zCVobSpot(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCVobSpot", zenkit::VirtualObjectType::zCVobSpot))
    return;
  }

void PropertyDelegate::mkIndex_zCVobStartpoint(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCVobStartpoint", zenkit::VirtualObjectType::zCVobStartpoint))
    return;
  }

void PropertyDelegate::mkIndex_zCMessageFilter(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCMessageFilter", zenkit::VirtualObjectType::zCMessageFilter))
    return;
  addView("target",       &zenkit::VMessageFilter::target);
  addView("on trigger",   &zenkit::VMessageFilter::on_trigger);
  addView("on untrigger", &zenkit::VMessageFilter::on_untrigger);
  }

void PropertyDelegate::mkIndex_zCCodeMaster(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCCodeMaster", zenkit::VirtualObjectType::zCCodeMaster))
    return;

  addView("target",                 &zenkit::VCodeMaster::target);
  addView("ordered",                &zenkit::VCodeMaster::ordered);
  addView("first false is failure", &zenkit::VCodeMaster::first_false_is_failure);
  addView("failure target",         &zenkit::VCodeMaster::failure_target);
  addView("untriggered cancels",    &zenkit::VCodeMaster::untriggered_cancels);
  addView("slaves",                 &zenkit::VCodeMaster::slaves, std::false_type());
  // addView("",                       &zenkit::VCodeMaster::s_num_triggered_slaves); // saves only
  }

void PropertyDelegate::mkIndex_zCTriggerWorldStart(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCTriggerWorldStart", zenkit::VirtualObjectType::zCTriggerWorldStart))
    return;
  addView("target",    &zenkit::VTriggerWorldStart::target);
  addView("fire once", &zenkit::VTriggerWorldStart::fire_once);
  // addView("fs_has_fired", &zenkit::VTriggerWorldStart::s_has_fired); // saves only
  }

void PropertyDelegate::mkIndex_zCCSCamera(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCCSCamera", zenkit::VirtualObjectType::zCCSCamera))
    return;
  }

void PropertyDelegate::mkIndex_zCCamTrj_KeyFrame(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCCamTrj_KeyFrame", zenkit::VirtualObjectType::zCCamTrj_KeyFrame))
    return;

  addView("time",                   &zenkit::VCameraTrajectoryFrame::time, 0.f, 6000.f);
  addView("roll angle",             &zenkit::VCameraTrajectoryFrame::roll_angle, -180.f, 180.f);
  addView("fov scale",              &zenkit::VCameraTrajectoryFrame::fov_scale, 0.f, 2.f);
  addView("motion type",            &zenkit::VCameraTrajectoryFrame::motion_type);
  addView("motion type fov",        &zenkit::VCameraTrajectoryFrame::motion_type_fov);
  addView("motion type roll",       &zenkit::VCameraTrajectoryFrame::motion_type_roll);
  addView("motion type time scale", &zenkit::VCameraTrajectoryFrame::motion_type_time_scale);
  addView("tension",                &zenkit::VCameraTrajectoryFrame::tension);
  addView("cam bias",               &zenkit::VCameraTrajectoryFrame::cam_bias);
  addView("continuity",             &zenkit::VCameraTrajectoryFrame::continuity);
  addView("time scale",             &zenkit::VCameraTrajectoryFrame::time_scale);
  addView("time fixed",             &zenkit::VCameraTrajectoryFrame::time_fixed);
  // addView("original_pose",          &zenkit::VCameraTrajectoryFrame::original_pose);
  }

void PropertyDelegate::mkIndex_oCTouchDamage(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("oCTouchDamage", zenkit::VirtualObjectType::oCTouchDamage))
    return;

  addView("damage",           &zenkit::VTouchDamage::damage, 0.f, 1000.f);
  addView("barrier",          &zenkit::VTouchDamage::barrier);
  addView("blunt",            &zenkit::VTouchDamage::blunt);
  addView("edge",             &zenkit::VTouchDamage::edge);
  addView("fire",             &zenkit::VTouchDamage::fire);
  addView("fly",              &zenkit::VTouchDamage::fly);
  addView("magic",            &zenkit::VTouchDamage::magic);
  addView("point",            &zenkit::VTouchDamage::point);
  addView("fall",             &zenkit::VTouchDamage::fall);
  addView("repeat delay sec", &zenkit::VTouchDamage::repeat_delay_sec, 0.f, 10.f);
  addView("volume scale",     &zenkit::VTouchDamage::volume_scale, 0.f, 3.f);
  addView("collision",        &zenkit::VTouchDamage::collision);
  }

void PropertyDelegate::mkIndex_zCTriggerUntouch(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCTriggerUntouch", zenkit::VirtualObjectType::zCTriggerUntouch))
    return;

  addView("target", &zenkit::VTriggerUntouch::target);
  }

void PropertyDelegate::mkIndex_zCEarthquake(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCEarthquake", zenkit::VirtualObjectType::zCEarthquake))
    return;

  addView("radius",    &zenkit::VEarthquake::radius, 0.f, 1000.f);
  addView("duration",  &zenkit::VEarthquake::duration);
  addView("amplitude", &zenkit::VEarthquake::amplitude);
  }

void PropertyDelegate::mkIndex_oCMOB(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("oCMOB", zenkit::VirtualObjectType::oCMOB))
    return;

  addView("name",             &zenkit::VMovableObject::name);
  addView("hp",               &zenkit::VMovableObject::hp,     0, 1000);
  addView("damage",           &zenkit::VMovableObject::damage, 0, 1000);
  addView("movable",          &zenkit::VMovableObject::movable);
  addView("takable",          &zenkit::VMovableObject::takable);
  addView("focus_override",   &zenkit::VMovableObject::focus_override);
  addView("material",         &zenkit::VMovableObject::material);
  addView("visual_destroyed", &zenkit::VMovableObject::visual_destroyed);
  addView("owner",            &zenkit::VMovableObject::owner);
  addView("owner_guild",      &zenkit::VMovableObject::owner_guild);
  addView("destroyed",        &zenkit::VMovableObject::destroyed);
  }

void PropertyDelegate::mkIndex_oCMobInter(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_oCMOB(type, vob);
  if(addHeader("oCMobInter", zenkit::VirtualObjectType::oCMobInter))
    return;
  addView("state count",              &zenkit::VInteractiveObject::state_count, 0, 20);
  addView("target",                   &zenkit::VInteractiveObject::target);
  addView("item",                     &zenkit::VInteractiveObject::item);
  addView("condition function",       &zenkit::VInteractiveObject::condition_function);
  addView("on state change function", &zenkit::VInteractiveObject::on_state_change_function);
  addView("rewind",                   &zenkit::VInteractiveObject::rewind);
  }

void PropertyDelegate::mkIndex_oCMobBed(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_oCMobInter(type, vob);
  if(addHeader("oCMobBed", zenkit::VirtualObjectType::oCMobBed))
    return;
  }

void PropertyDelegate::mkIndex_oCMobFire(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_oCMobInter(type, vob);
  if(addHeader("oCMobFire", zenkit::VirtualObjectType::oCMobFire))
    return;
  addView("state",                    &zenkit::VFire::state, 0, 10);
  addView("target",                   &zenkit::VFire::target);
  addView("item",                     &zenkit::VFire::item);
  addView("condition_function",       &zenkit::VFire::condition_function);
  addView("on_state_change_function", &zenkit::VFire::on_state_change_function);
  addView("rewind",                   &zenkit::VFire::rewind);
  }

void PropertyDelegate::mkIndex_oCMobLadder(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_oCMobInter(type, vob);
  if(addHeader("oCMobLadder", zenkit::VirtualObjectType::oCMobLadder))
    return;
  }

void PropertyDelegate::mkIndex_oCMobSwitch(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_oCMobInter(type, vob);
  if(addHeader("oCMobSwitch", zenkit::VirtualObjectType::oCMobSwitch))
    return;
  }

void PropertyDelegate::mkIndex_oCMobWheel(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_oCMobInter(type, vob);
  if(addHeader("oCMobWheel", zenkit::VirtualObjectType::oCMobWheel))
    return;
  }

void PropertyDelegate::mkIndex_oCMobContainer(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_oCMobInter(type, vob);
  if(addHeader("oCMobContainer", zenkit::VirtualObjectType::oCMobContainer))
    return;
  addView("locked",      &zenkit::VContainer::locked);
  addView("key",         &zenkit::VContainer::key);
  addView("pick string", &zenkit::VContainer::pick_string);
  addView("contents",    &zenkit::VContainer::contents); //FIXME: list of items
  }

void PropertyDelegate::mkIndex_oCMobDoor(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_oCMOB(type, vob);
  if(addHeader("oCMobDoor", zenkit::VirtualObjectType::oCMobDoor))
    return;
  addView("locked",      &zenkit::VDoor::locked);
  addView("key",         &zenkit::VDoor::key);
  addView("pick string", &zenkit::VDoor::pick_string);
  }

void PropertyDelegate::mkIndex_zCTrigger(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCTrigger", zenkit::VirtualObjectType::zCTrigger))
    return;
  addView("target",               &zenkit::VTrigger::target);
  addView("start enabled",        &zenkit::VTrigger::start_enabled);
  addView("send untrigger",       &zenkit::VTrigger::send_untrigger);
  addView("react to on trigger",  &zenkit::VTrigger::react_to_on_trigger);
  addView("react to on touch",    &zenkit::VTrigger::react_to_on_touch);
  addView("react to on damage",   &zenkit::VTrigger::react_to_on_damage);
  addView("respond to object",    &zenkit::VTrigger::respond_to_object);
  addView("respond to pc",        &zenkit::VTrigger::respond_to_pc);
  addView("respond to npc",       &zenkit::VTrigger::respond_to_npc);
  addView("vob target",           &zenkit::VTrigger::vob_target);
  addView("max activation count", &zenkit::VTrigger::max_activation_count, -1, 10);
  addView("retrigger delay sec",  &zenkit::VTrigger::retrigger_delay_sec, 0.f, 10.f);
  addView("damage threshold",     &zenkit::VTrigger::damage_threshold, 0.f, 10.f);
  addView("fire delay sec",       &zenkit::VTrigger::fire_delay_sec, 0.f, 10.f);
  }

void PropertyDelegate::mkIndex_zCTriggerList(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCTrigger(type, vob);
  if(addHeader("zCTriggerList", zenkit::VirtualObjectType::zCTriggerList))
    return;
  addView("mode",   &zenkit::VTriggerList::mode);
  addView("target", &zenkit::VTriggerList::targets, std::false_type());
  }

void PropertyDelegate::mkIndex_oCTriggerScript(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCTrigger(type, vob);
  if(addHeader("oCTriggerScript", zenkit::VirtualObjectType::oCTriggerScript))
    return;
  addView("function", &zenkit::VTriggerScript::function);
  }

void PropertyDelegate::mkIndex_oCTriggerChangeLevel(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCTrigger(type, vob);
  if(addHeader("oCTriggerChangeLevel", zenkit::VirtualObjectType::oCTriggerChangeLevel))
    return;
  addView("level name", &zenkit::VTriggerChangeLevel::level_name);
  addView("start vob",  &zenkit::VTriggerChangeLevel::start_vob);
  }

void PropertyDelegate::mkIndex_oCCSTrigger(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCTrigger(type, vob);
  if(addHeader("oCCSTrigger", zenkit::VirtualObjectType::oCCSTrigger))
    return;
  }

void PropertyDelegate::mkIndex_zCMover(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCTrigger(type, vob);
  if(addHeader("zCMover", zenkit::VirtualObjectType::zCMover))
    return;
  addView("behavior",             &zenkit::VMover::behavior);
  addView("touch blocker damage", &zenkit::VMover::touch_blocker_damage);
  addView("stay open time sec",   &zenkit::VMover::stay_open_time_sec, 0.f, 10.f);
  addView("locked",               &zenkit::VMover::locked);
  addView("auto link",            &zenkit::VMover::auto_link);
  addView("auto rotate",          &zenkit::VMover::auto_rotate);
  addView("speed",                &zenkit::VMover::speed);
  addView("lerp mode",            &zenkit::VMover::lerp_mode);
  addView("speed mode",           &zenkit::VMover::speed_mode);
  addView("keyframes",            &zenkit::VMover::keyframes, std::false_type());
  addView("sfx open start",       &zenkit::VMover::sfx_open_start);
  addView("sfx open end",         &zenkit::VMover::sfx_open_end);
  addView("sfx transitioning",    &zenkit::VMover::sfx_transitioning);
  addView("sfx close start",      &zenkit::VMover::sfx_close_start);
  addView("sfx close end",        &zenkit::VMover::sfx_close_end);
  addView("sfx lock",             &zenkit::VMover::sfx_lock);
  addView("sfx unlock",           &zenkit::VMover::sfx_unlock);
  addView("sfx use locked",       &zenkit::VMover::sfx_use_locked);
  }

void PropertyDelegate::mkIndex_zCVobSound(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCVobSound", zenkit::VirtualObjectType::zCVobSound))
    return;
  addView("volume",                 &zenkit::VSound::volume, 0.f, 10.f);
  addView("mode",                   &zenkit::VSound::mode);
  addView("random delay",           &zenkit::VSound::random_delay,     0.f, 10.f);
  addView("random delay variation", &zenkit::VSound::random_delay_var, 0.f, 10.f);
  addView("initially playing",      &zenkit::VSound::initially_playing);
  addView("ambient3d",              &zenkit::VSound::ambient3d);
  addView("obstruction",            &zenkit::VSound::obstruction);
  addView("cone angle",             &zenkit::VSound::cone_angle);
  addView("volume type",            &zenkit::VSound::volume_type);
  addView("radius",                 &zenkit::VSound::radius);
  addView("sound name",             &zenkit::VSound::sound_name);
  }

void PropertyDelegate::mkIndex_zCVobSoundDaytime(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVobSound(type, vob);
  if(addHeader("zCVobSoundDaytime", zenkit::VirtualObjectType::zCVobSound))
    return;
  addView("start_time",  &zenkit::VSoundDaytime::start_time, 0.f, 24.f);
  addView("end_time",    &zenkit::VSoundDaytime::end_time,   0.f, 24.f);
  addView("sound_name2", &zenkit::VSoundDaytime::sound_name2);
  }

void PropertyDelegate::mkIndex_oCZoneMusic(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("oCZoneMusic", zenkit::VirtualObjectType::oCZoneMusic))
    return;
  addView("enabled",   &zenkit::VZoneMusic::enabled);
  addView("priority",  &zenkit::VZoneMusic::priority);
  addView("ellipsoid", &zenkit::VZoneMusic::ellipsoid);
  addView("reverb",    &zenkit::VZoneMusic::reverb, 0.f, 2.f);
  addView("volume",    &zenkit::VZoneMusic::volume, 0.f, 2.f);
  addView("loop",      &zenkit::VZoneMusic::loop);
  }

void PropertyDelegate::mkIndex_oCZoneMusicDefault(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_oCZoneMusic(type, vob);
  if(addHeader("oCZoneMusicDefault", zenkit::VirtualObjectType::oCZoneMusic))
    return;
  }

void PropertyDelegate::mkIndex_zCZoneZFog(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCZoneZFog", zenkit::VirtualObjectType::zCZoneZFog))
    return;
  addView("range center",           &zenkit::VZoneFog::range_center, 0.f, 1000.f);
  addView("inner range percentage", &zenkit::VZoneFog::inner_range_percentage, 0.f, 100.f);
  addView("color",                  &zenkit::VZoneFog::color);
  addView("fade out sky",           &zenkit::VZoneFog::fade_out_sky);
  addView("override color",         &zenkit::VZoneFog::override_color);
  }

void PropertyDelegate::mkIndex_zCZoneZFogDefault(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCZoneZFog(type, vob);
  if(addHeader("zCZoneZFogDefault", zenkit::VirtualObjectType::zCZoneZFogDefault))
    return;
  }

void PropertyDelegate::mkIndex_zCZoneVobFarPlane(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  if(addHeader("zCZoneVobFarPlane", zenkit::VirtualObjectType::zCZoneVobFarPlane))
    return;
  addView("vob far plane z",        &zenkit::VZoneFarPlane::vob_far_plane_z,        0.f, 1.f);
  addView("inner range percentage", &zenkit::VZoneFarPlane::inner_range_percentage, 0.f, 100.f);
  }

void PropertyDelegate::mkIndex_zCZoneVobFarPlaneDefault(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCZoneVobFarPlane(type, vob);
  if(addHeader("zCZoneVobFarPlaneDefault", zenkit::VirtualObjectType::zCZoneVobFarPlaneDefault))
    return;
  }
