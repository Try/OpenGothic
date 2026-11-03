#pragma once

#include <Tempest/ListDelegate>

#include "objects/worldedit.h"
#include "ui/property/property.h"

class PropertyDelegate : public Tempest::ListDelegate {
  public:
    PropertyDelegate();

    void             setVob(WorldEdit::Vob* vob);
    void             update();

    std::function<void(std::unique_ptr<Command::Action<WorldEdit>>&,bool)> onChanged;

    size_t           size() const override;
    Tempest::Widget* createView(size_t position) override;

  private:
    struct TypeHeader;
    struct Index {
      Property::Slot            slt;
      zenkit::VirtualObjectType cat = {};

      std::function<Variant(const WorldEdit::Vob*)> get;
      std::function<void(WorldEdit::Vob*,const Variant&,bool)>  set;
      };

    void onProperty(size_t id, const Variant& v, bool commit);
    void onCategory(zenkit::VirtualObjectType cat);

    bool addHeader(std::string_view name, zenkit::VirtualObjectType type);
    template<class T, class F>
    auto addView(std::string_view name, F T::*) -> Index&;
    template<class T, class F>
    auto addView(std::string_view name, F T::*, F min, F max) -> Index&;
    template<class T, class F>
    auto addView(std::string_view name, F T::*, std::false_type) -> Index&;

    bool isClosed(zenkit::VirtualObjectType type) const;
    void mkIndex(const zenkit::VirtualObject* vob);
    void mkIndex(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);

    void mkIndex_zCVob(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCVobLevelCompo(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCItem(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCMoverController(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCVobScreenFX(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCVobStair(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCPFXController(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCEffect(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCVobAnimate(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCVobLensFlare(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCVobLight(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCVobSpot(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCVobStartpoint(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCMessageFilter(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCCodeMaster(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCTriggerWorldStart(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCCSCamera(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCCamTrj_KeyFrame(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCTouchDamage(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCTriggerUntouch(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCEarthquake(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCMOB(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCMobInter(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCMobBed(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCMobFire(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCMobLadder(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCMobSwitch(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCMobWheel(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCMobContainer(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCMobDoor(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCTrigger(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCTriggerList(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCTriggerScript(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCTriggerChangeLevel(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCCSTrigger(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCMover(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCVobSound(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCVobSoundDaytime(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCZoneMusic(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_oCZoneMusicDefault(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCZoneZFog(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCZoneZFogDefault(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCZoneVobFarPlane(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);
    void mkIndex_zCZoneVobFarPlaneDefault(zenkit::VirtualObjectType type, const zenkit::VirtualObject& vob);

    std::unordered_set<zenkit::VirtualObjectType> closed;
    std::vector<Index>                            index;
    WorldEdit::Vob*                               vob = nullptr;
  };
