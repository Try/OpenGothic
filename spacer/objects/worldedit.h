#pragma once

#include <string_view>
#include <memory>

#include <zenkit/vobs/VirtualObject.hh>

#include "physics/physicmesh.h"
#include "graphics/mdlvisual.h"
#include "graphics/lightgroup.h"
#include "graphics/meshobjects.h"
#include "graphics/mesh/pose.h"
#include "graphics/objvisual.h"
#include "command.h"

class DynamicWorld;
class WorldView;

class WorldEdit {
  public:
    WorldEdit(std::string_view wname);
    ~WorldEdit();

    WorldView& view() { return *wview; }
    const DynamicWorld& dynamic() const { return *physics; }

    class Vob {
      public:
        Vob() { orig = std::make_shared<zenkit::VirtualObject>(); orig->type=zenkit::VirtualObjectType::zCVob; }
        Vob(std::shared_ptr<zenkit::VirtualObject> vob);

        const zenkit::VirtualObject* get() const { return orig.get(); }
        const zenkit::VirtualObject& operator *  () const { return *orig; }
        const zenkit::VirtualObject* operator -> () const { return orig.get(); }

        size_t size() const { return child.size(); }
        auto& operator[](size_t i) const { return *child[i]; }
        auto& operator[](size_t i)       { return *child[i]; }

        auto release(size_t i) -> std::unique_ptr<Vob>;
        void insert(size_t i, std::unique_ptr<Vob> v);

        Tempest::Vec3 position() const;
        zenkit::Mat3  rotation() const;

        void setPosition(const Tempest::Vec3& pos);
        void setRotation(const zenkit::Mat3&  rot);

        void setVisual(WorldEdit& owner, std::string_view vis);
        void setCollision(WorldEdit& owner, bool cd);

      private:
        void clearView();
        void initView(WorldEdit& owner);

        std::vector<std::unique_ptr<Vob>>      child;
        std::shared_ptr<zenkit::VirtualObject> orig;
        bool                                   physics = true;

        PhysicMesh        phys;
        MeshObjects::Mesh mesh;
        MdlVisual         mdl;
        LightGroup::Light light;

      friend class WorldEdit;

      friend class CmdNewVob;
      friend class CmdDeleteVob;

      friend class CmdSetProperty;
      };

    const Vob& root() const { return rootVob; }
    Vob&       root() { return rootVob; }

    bool       isValidVob(const Vob* v) const;

  private:
    void load(Vob& out, std::vector<std::shared_ptr<zenkit::VirtualObject>>& child);
    bool isValidVob(const Vob& node, const Vob* v) const;

    std::unique_ptr<DynamicWorld> physics;
    std::unique_ptr<WorldView>    wview;
    Vob                           rootVob {0};
  };

class CmdNewVob : public Command::Action<WorldEdit> {
  public:
    CmdNewVob(WorldEdit::Vob* vob);

  private:
    void redo(WorldEdit& subj) override;
    void undo(WorldEdit& subj) override;

    WorldEdit::Vob* findParent(WorldEdit::Vob& v, const WorldEdit::Vob* dst);

    WorldEdit::Vob*                 vob    = nullptr;
    WorldEdit::Vob*                 parent = nullptr;
    std::unique_ptr<WorldEdit::Vob> stash;
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

class CmdRotateVob : public Command::Action<WorldEdit> {
  public:
    CmdRotateVob(WorldEdit::Vob* vob, zenkit::Mat3 ang);

  private:
    void redo(WorldEdit& subj) override;
    void undo(WorldEdit& subj) override;
    bool merge(const Action& prev) override;

    WorldEdit::Vob* vob = nullptr;
    zenkit::Mat3    ang, orig;
  };
