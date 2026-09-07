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

        void clearView();
        void initView(WorldEdit& owner);

        void setPosition(const Tempest::Vec3& pos);

      private:
        std::vector<std::unique_ptr<Vob>>      child;
        std::shared_ptr<zenkit::VirtualObject> orig;

        PhysicMesh        phys;
        MeshObjects::Mesh mesh;
        LightGroup::Light light;

      friend class WorldEdit;
      };

    const Vob& root() const { return rootVob; }
    Vob&       root() { return rootVob; }

    Vob* rayQuery(const Tempest::Vec3 s, const Tempest::Vec3 e);
    Vob* rayQuery(Tempest::Matrix4x4 view, Tempest::Matrix4x4 vp, Tempest::Point mpos, Tempest::Size wsize);

  private:
    void load(Vob& out, std::vector<std::shared_ptr<zenkit::VirtualObject>>& child);
    void initView(Vob& out);

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