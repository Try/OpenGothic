#include "propertydelegate.h"

#include <zenkit/vobs/Misc.hh>

#include "ui/controls/parameterwidget.h"
#include "ui/controls/widgetheader.h"
#include "ui/property/property.h"

using namespace Tempest;

PropertyDelegate::PropertyDelegate() {
  }

void PropertyDelegate::setVob(const WorldEdit::Vob* inVob) {
  vob = inVob;
  mkIndex(vob->get());
  invalidateView();
  }

size_t PropertyDelegate::size() const {
  return index.size();
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
  return ParameterWidget::createEditor(index[i].slt, var, i);
  }

void PropertyDelegate::addHeader(std::string_view name) {
  Index id;
  id.slt.name = name;
  id.slt.type = nullptr;
  index.push_back(id);
  }

template<class T, class F>
void PropertyDelegate::addView(std::string_view name, F T::* field) {
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

  id.get = [field](const WorldEdit::Vob* vob) -> Variant {
    if(auto d = dynamic_cast<const T*>(vob->get())) {
      auto val = (*d.*field);
      return Variant(val);
      }
    return Variant();
    };
  index.push_back(id);
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
    case zenkit::VirtualObjectType::zCVobLensFlare:
    case zenkit::VirtualObjectType::zCVobLight:
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
