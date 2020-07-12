#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

#include <zenload/zTypes.h>

class World;

class Vob {
  public:
    Vob()=default;
    Vob(World& owner, ZenLoad::zCVobData& vob, bool startup);
    virtual ~Vob();
    static std::unique_ptr<Vob> load(World& owner, ZenLoad::zCVobData&& vob, bool startup);

    Tempest::Vec3 position() const;
    auto          transform() const -> const Tempest::Matrix4x4& { return pos; }
    void          setTransform(const Tempest::Matrix4x4& p);

  private:
    std::vector<std::unique_ptr<Vob>> child;
    Tempest::Matrix4x4                pos;
  };

