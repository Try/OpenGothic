#pragma once

#include <string_view>
#include <memory>
#include <unordered_map>

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

#include <zenkit/vobs/VirtualObject.hh>

#include "graphics/meshobjects.h"

class DynamicWorld;
class WorldView;
class StaticMesh;

class WorldEdit {
  public:
    WorldEdit(std::string_view wname);
    ~WorldEdit();

    WorldView& view() { return *wview; }

    bool                    select(const Tempest::Vec3& rayOrigin, const Tempest::Vec3& rayDirection, float maxDistance);
    bool                    hasSelection() const;
    Tempest::Matrix4x4      selectedTransform() const;
    void                    setSelectedTransform(const Tempest::Matrix4x4& transform);
    std::string_view        selectedName() const;

  private:
    struct Vob {
      std::vector<Vob>                       child;
      std::shared_ptr<zenkit::VirtualObject> orig;

      MeshObjects::Mesh mesh;
      Tempest::Matrix4x4 transform;
      };

    struct PickMesh;

    void load(Vob& out, std::vector<std::shared_ptr<zenkit::VirtualObject>>& child);
    void initView(Vob& out);
    const PickMesh& pickMesh(const StaticMesh& mesh);
    bool rayMesh(Vob& vob, const Tempest::Vec3& rayOrigin, const Tempest::Vec3& rayDirection, float& distance);

    Vob                           root;
    std::vector<Vob*>             selectable;
    Vob*                          selected = nullptr;
    std::unordered_map<const StaticMesh*,std::shared_ptr<PickMesh>> pickMeshes;
    std::unique_ptr<DynamicWorld> physics;
    std::unique_ptr<WorldView>    wview;
  };
