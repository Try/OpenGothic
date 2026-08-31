#pragma once

#include <string_view>
#include <memory>

#include <zenkit/vobs/VirtualObject.hh>

#include "physics/physicmesh.h"
#include "graphics/meshobjects.h"

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
        const zenkit::VirtualObject& operator -> () const { return *orig; }

        size_t size() const { return child.size(); }
        const Vob& operator[](size_t i) const { return child[i]; }

      private:
        std::vector<Vob>                       child;
        std::shared_ptr<zenkit::VirtualObject> orig;

        MeshObjects::Mesh mesh;
        PhysicMesh        phys;

      friend class WorldEdit;
      };

    const Vob& root() const { return rootVob; }

    Vob* rayQuery(const Tempest::Vec3 s, const Tempest::Vec3 e);

  private:
    void load(Vob& out, std::vector<std::shared_ptr<zenkit::VirtualObject>>& child);
    void initView(Vob& out);

    Vob* validatePointer(const zenkit::VirtualObject* ptr, Vob& v);

    std::unique_ptr<DynamicWorld> physics;
    std::unique_ptr<WorldView>    wview;
    Vob                           rootVob {0};
    size_t                        vobNextId = 1;
  };

