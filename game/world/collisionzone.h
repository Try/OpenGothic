#pragma once

#include <Tempest/Vec>

#include <functional>
#include <vector>

class World;
class ParticleFx;
class Serialize;
class Npc;

class CollisionZone final {
  public:
    CollisionZone();
    CollisionZone(World& owner, const Tempest::Vec3& pos, const ParticleFx& pfx);
    CollisionZone(World& owner, const Tempest::Vec3& pos, const Tempest::Vec3& size);
    CollisionZone(CollisionZone&& other);
    CollisionZone& operator = (CollisionZone&& other);
    ~CollisionZone();

    void          save(Serialize& fout) const;
    void          load(Serialize &fin);

    void          setCallback(std::function<void(Npc& npc)> f);

    Tempest::Vec3 position() const { return pos; }
    void          setPosition(const Tempest::Vec3& p);

    const std::vector<Npc*>& intersections() const { return intersect; }

    bool          checkPos(const Tempest::Vec3& pos) const;
    void          onIntersect(Npc& npc);
    void          tick(uint64_t dt);

  private:
    World*                    owner = nullptr;
    std::function<void(Npc&)> cb;

    enum Type:uint8_t {
      T_BBox,
      T_Capsule,
      };
    uint64_t          time0 = 0;
    Type              type = T_BBox;
    Tempest::Vec3     pos, size;
    const ParticleFx* pfx = nullptr;

    std::vector<Npc*> intersect;
  };

