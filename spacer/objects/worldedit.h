#pragma once

#include <string_view>
#include <memory>

#include <zenkit/vobs/VirtualObject.hh>

#include "physics/physicmesh.h"
#include "graphics/lightgroup.h"
#include "graphics/meshobjects.h"
#include "command.h"

class DynamicWorld;
class WorldView;

class WorldEdit {
  public:
    WorldEdit(std::string_view wname);
    ~WorldEdit();

    WorldView& view() { return *wview; }

    class Vob {
      public:
        Vob(uint64_t id):id(id){}
        const uint64_t id;

        const zenkit::VirtualObject* get() const { return orig.get(); }
        const zenkit::VirtualObject& operator *  () const { return *orig; }
        const zenkit::VirtualObject* operator -> () const { return orig.get(); }

        size_t size() const { return child.size(); }
        auto& operator[](size_t i) const { return *child[i]; }
        auto& operator[](size_t i)       { return *child[i]; }

        auto release(size_t i) -> std::unique_ptr<Vob>;
        void insert(size_t i, std::unique_ptr<Vob> v);

        void setPosition(const Tempest::Vec3& pos);

      private:
        void clearView();
        void initView(WorldEdit& owner);

        std::vector<std::unique_ptr<Vob>>      child;
        std::shared_ptr<zenkit::VirtualObject> orig;

        PhysicMesh        phys;
        MeshObjects::Mesh mesh;
        LightGroup::Light light;

      friend class WorldEdit;

      friend class CmdDeleteVob;

      template<class Vob, class F>
      friend class CmdSetProperty;
      };

    const Vob& root() const { return rootVob; }
    Vob&       root() { return rootVob; }

    Vob* rayQuery(const Tempest::Vec3 s, const Tempest::Vec3 e);
    Vob* rayQuery(Tempest::Matrix4x4 view, Tempest::Matrix4x4 vp, Tempest::Point mpos, Tempest::Size wsize);

  private:
    void load(Vob& out, std::vector<std::shared_ptr<zenkit::VirtualObject>>& child);

    void rayQueryLight(Tempest::Point mpos, Tempest::Size wsize, const Tempest::Matrix4x4& vp,
                       const Tempest::Vec3& rayOrig, const Tempest::Vec3& rayDir,
                       float& rayT, Vob*& ret, Vob& v);

    Vob* validatePointer(const zenkit::VirtualObject* ptr, Vob& v);

    std::unique_ptr<DynamicWorld> physics;
    std::unique_ptr<WorldView>    wview;
    Vob                           rootVob {0};
    size_t                        vobNextId = 1;
  };

class CmdMoveVob : public Command::Action<WorldEdit> {
  public:
    CmdMoveVob(WorldEdit::Vob* vob, Tempest::Vec3 pos);

  private:
    void redo(WorldEdit& subj) override;
    void undo(WorldEdit& subj) override;
    bool merge(const Action& prev) override;

    WorldEdit::Vob* vob = nullptr;
    Tempest::Vec3   pos, orig;
  };

class CmdDeleteVob : public Command::Action<WorldEdit> {
  public:
    CmdDeleteVob(WorldEdit::Vob* vob);

  private:
    void redo(WorldEdit& subj) override;
    void undo(WorldEdit& subj) override;

    WorldEdit::Vob* findParent(WorldEdit::Vob& v, const WorldEdit::Vob* dst);

    WorldEdit::Vob*                 vob    = nullptr;
    WorldEdit::Vob*                 parent = nullptr;
    size_t                          index  = 0;
    std::unique_ptr<WorldEdit::Vob> stash;
  };

template<class Vob, class F>
class CmdSetProperty: public Command::Action<WorldEdit> {
  public:
    CmdSetProperty(WorldEdit::Vob* vob, F Vob::* field, const F& v) : vob(vob), field(field), value(v) {}

  private:
    void redo(WorldEdit& subj) override {
      auto& vx = *dynamic_cast<Vob*>(vob->orig.get());
      auto& f  = (vx.*field);
      std::swap(f, value);
      //TODO: easeup non-visivic properties
      //TODO: easeup non-committed props
      vob->initView(subj);
      }

    void undo(WorldEdit& subj) override {
      redo(subj);
      }

    bool merge(const Action& prev) override {
      if(auto p = dynamic_cast<const CmdSetProperty<Vob,F>*>(&prev)) {
        if(p->vob==vob && p->field==field) {
          return true;
          }
        }
      return false;
      }

    WorldEdit::Vob* vob;
    F Vob::*        field;
    F               value;
  };