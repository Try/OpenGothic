#include "propertydelegate.h"

#include <Tempest/Log>

#include <zenkit/vobs/Misc.hh>
#include <zenkit/vobs/Light.hh>

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
  Tempest::Log::d("failed variant cast (", typeid(T).name(), ")");
  return T();
  }

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

Widget* PropertyDelegate::createView(size_t i) {
  if(index[i].slt.type==nullptr) {
    auto ret = new WidgetHeader();
    ret->setSizePolicy(Preferred,Fixed);
    ret->setText(index[i].slt.name);
    ret->setClosed(false);
    return ret;
    }
  auto var = index[i].get(vob);
  auto ed  =  ParameterWidget::createEditor(index[i].slt, var, i);
  ed->onChanged.bind(this, &PropertyDelegate::onProperty);
  return ed;
  }

void PropertyDelegate::addHeader(std::string_view name) {
  Index id;
  id.slt.name = name;
  id.slt.type = nullptr;
  index.push_back(id);
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
    case zenkit::VirtualObjectType::zCVobStartpoint:
    case zenkit::VirtualObjectType::zCMessageFilter:
    case zenkit::VirtualObjectType::zCCodeMaster:
    case zenkit::VirtualObjectType::zCTriggerWorldStart:
    case zenkit::VirtualObjectType::zCCSCamera:
    case zenkit::VirtualObjectType::zCCamTrj_KeyFrame:
    case zenkit::VirtualObjectType::oCTouchDamage:
    case zenkit::VirtualObjectType::zCTriggerUntouch:
    case zenkit::VirtualObjectType::zCEarthquake:
    case zenkit::VirtualObjectType::oCMOB:
    case zenkit::VirtualObjectType::oCMobInter:
    case zenkit::VirtualObjectType::oCMobBed:
    case zenkit::VirtualObjectType::oCMobFire:
    case zenkit::VirtualObjectType::oCMobLadder:
    case zenkit::VirtualObjectType::oCMobSwitch:
    case zenkit::VirtualObjectType::oCMobWheel:
    case zenkit::VirtualObjectType::oCMobContainer:
    case zenkit::VirtualObjectType::oCMobDoor:
    case zenkit::VirtualObjectType::zCTrigger:
    case zenkit::VirtualObjectType::zCTriggerList:
    case zenkit::VirtualObjectType::oCTriggerScript:
    case zenkit::VirtualObjectType::oCTriggerChangeLevel:
    case zenkit::VirtualObjectType::oCCSTrigger:
    case zenkit::VirtualObjectType::zCMover:
    case zenkit::VirtualObjectType::zCVobSound:
    case zenkit::VirtualObjectType::zCVobSoundDaytime:
    case zenkit::VirtualObjectType::oCZoneMusic:
    case zenkit::VirtualObjectType::oCZoneMusicDefault:
    case zenkit::VirtualObjectType::zCZoneZFog:
    case zenkit::VirtualObjectType::zCZoneZFogDefault:
    case zenkit::VirtualObjectType::zCZoneVobFarPlane:
    case zenkit::VirtualObjectType::zCZoneVobFarPlaneDefault:
      break;
    }
  }

void PropertyDelegate::mkIndex_zCVob(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  addHeader("zCVob");
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
  addView("zbias",                 &zenkit::VirtualObject::bias);
  addView("isAmbient",             &zenkit::VirtualObject::ambient);
  }

void PropertyDelegate::mkIndex_zCVobLevelCompo(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("zCVobLevelCompo");
  }

void PropertyDelegate::mkIndex_oCItem(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("oCItem");
  addView("itemInstance", &zenkit::VItem::instance);
  }

void PropertyDelegate::mkIndex_zCMoverController(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("zCMoverController");
  addView("target",  &zenkit::VMoverController::target);
  addView("message", &zenkit::VMoverController::message);
  addView("key",     &zenkit::VMoverController::key);
  }

void PropertyDelegate::mkIndex_zCVobScreenFX(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("VScreenEffect");
  }

void PropertyDelegate::mkIndex_zCVobStair(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("VStair");
  }

void PropertyDelegate::mkIndex_zCPFXController(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("zCPFXController");
  addView("pfxName",         &zenkit::VParticleEffectController::pfx_name);
  addView("killVobWhenDone", &zenkit::VParticleEffectController::kill_when_done);
  addView("pfxStartOn",      &zenkit::VParticleEffectController::initially_running);
  }

void PropertyDelegate::mkIndex_zCEffect(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("zCEffect");
  }

void PropertyDelegate::mkIndex_zCVobAnimate(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCEffect(type, vob);
  addHeader("zCVobAnimate");
  addView("startOn", &zenkit::VAnimate::start_on);
  }

void PropertyDelegate::mkIndex_zCVobLensFlare(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCEffect(type, vob);
  addHeader("zCVobLensFlare");
  addView("lensflareFX", &zenkit::VLensFlare::fx);
  }

void PropertyDelegate::mkIndex_zCVobLight(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob) {
  mkIndex_zCVob(type, vob);
  addHeader("zCVobLight");
  addView("lightPresetInUse", &zenkit::VLight::preset);
  addView("lightType",        &zenkit::VLight::light_type);
  addView("range",            &zenkit::VLight::range, 0.f, 2000.f);
  addView("color",            &zenkit::VLight::color);
  addView("spotConeAngle",    &zenkit::VLight::cone_angle, 0.f, 180.f);
  addView("lightStatic",      &zenkit::VLight::is_static);
  addView("lightQuality",     &zenkit::VLight::quality);
  addView("lensflareFX",      &zenkit::VLight::lensflare_fx);

  //addView("turnedOn",         &zenkit::VLight::on);
  }
