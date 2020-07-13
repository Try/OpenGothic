#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

#include <zenload/zTypes.h>

class World;

class Vob {
  public:
    Vob(World& owner);
    Vob(Vob* parent, World& owner, ZenLoad::zCVobData& vob, bool startup);
    virtual ~Vob();
    static std::unique_ptr<Vob> load(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup);

    Tempest::Vec3 position() const;
    auto          transform() const -> const Tempest::Matrix4x4& { return pos; }
    void          setGlobalTransform(const Tempest::Matrix4x4& p);

    auto          localTransform() const -> const Tempest::Matrix4x4& { return local; }
    void          setLocalTransform(const Tempest::Matrix4x4& p);

  protected:
    World&                            world;

    virtual void  moveEvent();

  private:
    std::vector<std::unique_ptr<Vob>> child;
    Tempest::Matrix4x4                pos, local;
    Vob*                              parent = nullptr;

    void          recalculateTransform();
  };

