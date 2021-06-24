#pragma once

#include <Tempest/Vec>
#include <functional>

class World;
class Npc;

class CollisionZone final {
  public:
    CollisionZone();
    CollisionZone(World& owner, const Tempest::Vec3& pos, float R);
    CollisionZone(World& owner, const Tempest::Vec3& pos, const Tempest::Vec3& size);
    CollisionZone(CollisionZone&& other);
    CollisionZone& operator = (CollisionZone&& other);
    ~CollisionZone();

    void          setCallback(std::function<void(Npc& npc)> f);

    Tempest::Vec3 position() const { return pos; }
    void          setPosition(const Tempest::Vec3& p);

    bool          checkPos(const Tempest::Vec3& pos) const;
    void          onIntersect(Npc& npc);

  private:
    World*                    owner = nullptr;
    std::function<void(Npc&)> cb;

    enum Type:uint8_t {
      T_BBox,
      T_Capsule,
      };
    Tempest::Vec3 pos, size;
    Type          type = T_BBox;
  };

