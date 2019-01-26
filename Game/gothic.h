#pragma once

#include <string>
#include <memory>

#include <Tempest/Signal>
#include <daedalus/DaedalusVM.h>

#include "world/world.h"

class Gothic final {
  public:
    Gothic(int argc,const char** argv);

    bool isInGame() const;

    void setWorld(World&& w);
    const World& world() const { return wrld; }

    Tempest::Signal<void(const std::string&)> onSetWorld;

    const std::string&                    path() const { return gpath; }
    const std::string&                    defaultWorld() const;
    std::unique_ptr<Daedalus::DaedalusVM> createVm(const char* datFile);

    static void debug(const ZenLoad::zCMesh &mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedMesh& mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedSkeletalMesh& mesh, std::ostream& out);

  private:
    std::string wdef, gpath;

    World       wrld;
  };
