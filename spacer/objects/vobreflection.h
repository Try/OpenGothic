#pragma once

#include <functional>
#include <span>
#include <type_traits>
#include <unordered_map>

#include <zenkit/vobs/VirtualObject.hh>

#include "ui/property/property.h"

class VobReflection final {
  public:
    using Type = zenkit::VirtualObjectType;

    struct Field {
      Property::Slot slot;
      std::function<Variant(const zenkit::VirtualObject&)> get;
      // Replaces the field and stores its previous value in the argument.
      std::function<bool(zenkit::VirtualObject&, Variant&)> exchange;
      };

    class Class {
      public:
        Type             type;
        std::string_view name;
        Type             baseType;

        const Class*           baseClass() const;
        std::span<const Field> properties() const;
        // Both inheritance predicates exclude the class itself.
        bool                   isDerivedOf(Type base) const;
        bool                   isBaseOf(Type derived) const;

      private:
        Class(Type type, std::string_view name, Type baseType)
          : type(type), name(name), baseType(baseType) {}

        std::vector<Field> fields;

      friend class VobReflection;
      };

    // Descriptors have static lifetime. Unknown types return nullptr.
    static const Class* find(Type type);

  private:
    VobReflection();

    static const VobReflection& instance();

    Class& addClass(Type type, std::string_view name, Type baseType);

    template<class T, class F>
    static Field& addProperty(std::vector<Field>& fields, std::string_view name, F T::* field);
    template<class T, class F>
    static Field& addProperty(std::vector<Field>& fields, std::string_view name, F T::* field, F min, F max);
    template<class T, class F>
    static Field& addProperty(std::vector<Field>& fields, std::string_view name, F T::*, std::false_type);

    void register_zCVob();
    void register_zCVobLevelCompo();
    void register_oCItem();
    void register_oCNpc();
    void register_zCMoverController();
    void register_zCVobScreenFX();
    void register_zCVobStair();
    void register_zCPFXController();
    void register_zCVobAnimate();
    void register_zCVobLensFlare();
    void register_zCVobLight();
    void register_zCVobSpot();
    void register_zCVobStartpoint();
    void register_zCMessageFilter();
    void register_zCCodeMaster();
    void register_zCTriggerWorldStart();
    void register_zCCSCamera();
    void register_zCCamTrj_KeyFrame();
    void register_oCTouchDamage();
    void register_zCTriggerUntouch();
    void register_zCEarthquake();
    void register_oCMOB();
    void register_oCMobInter();
    void register_oCMobBed();
    void register_oCMobFire();
    void register_oCMobLadder();
    void register_oCMobSwitch();
    void register_oCMobWheel();
    void register_oCMobContainer();
    void register_oCMobDoor();
    void register_zCTrigger();
    void register_zCTriggerList();
    void register_oCTriggerScript();
    void register_oCTriggerChangeLevel();
    void register_oCCSTrigger();
    void register_zCMover();
    void register_zCVobSound();
    void register_zCVobSoundDaytime();
    void register_oCZoneMusic();
    void register_oCZoneMusicDefault();
    void register_zCZoneZFog();
    void register_zCZoneZFogDefault();
    void register_zCZoneVobFarPlane();
    void register_zCZoneVobFarPlaneDefault();

    std::unordered_map<Type, Class> classes;
  };
