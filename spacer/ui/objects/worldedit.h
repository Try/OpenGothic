#pragma once

#include <string_view>
#include <memory>

#include <zenkit/vobs/VirtualObject.hh>

class DynamicWorld;

class WorldEdit {
  public:
    WorldEdit(std::string_view wname);
    ~WorldEdit();

  private:
    struct Vob {
      std::vector<Vob>                       child;
      std::shared_ptr<zenkit::VirtualObject> orig;
      };

    void load(Vob& out, std::vector<std::shared_ptr<zenkit::VirtualObject>>& child);

    Vob                           root;
    std::unique_ptr<DynamicWorld> physics;
  };

