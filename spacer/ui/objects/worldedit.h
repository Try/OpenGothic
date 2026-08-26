#pragma once

#include <string_view>
#include <memory>

#include <zenkit/vobs/VirtualObject.hh>

#include "graphics/meshobjects.h"

class DynamicWorld;
class WorldView;

class WorldEdit {
  public:
    WorldEdit(std::string_view wname);
    ~WorldEdit();

    WorldView& view() { return *wview; }

  private:
    struct Vob {
      std::vector<Vob>                       child;
      std::shared_ptr<zenkit::VirtualObject> orig;

      MeshObjects::Mesh mesh;
      };

    void load(Vob& out, std::vector<std::shared_ptr<zenkit::VirtualObject>>& child);
    void initView(Vob& out);

    Vob                           root;
    std::unique_ptr<DynamicWorld> physics;
    std::unique_ptr<WorldView>    wview;
  };

