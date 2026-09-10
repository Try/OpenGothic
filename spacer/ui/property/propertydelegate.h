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
    struct Index {
      Property::Slot slt;
      std::function<Variant(const WorldEdit::Vob*)> get;
      std::function<void(WorldEdit::Vob*,const Variant&,bool)>  set;
      };

    void onProperty(size_t id, const Variant& v, bool commit);

    void addHeader(std::string_view name);
    template<class T, class F>
    auto addView(std::string_view name, F T::*) -> Index&;
    template<class T, class F>
    auto addView(std::string_view name, F T::*, F min, F max) -> Index&;

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

    std::vector<Index> index;
    WorldEdit::Vob*    vob = nullptr;
  };
