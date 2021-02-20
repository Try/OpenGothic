#pragma once

#include <vector>
#include <list>

#include "resources.h"

class ParticleFx;
class SceneGlobals;
class VisualObjects;
class World;

class TrlObjects {
  private:
    struct Point;
    struct Trail;
    struct Bucket;
    enum AllocState: uint8_t {
      S_Free,
      S_Alloc,
      S_Fade,
      };

  public:
    using Vertex = Resources::Vertex;

    TrlObjects(const SceneGlobals& scene, VisualObjects& visual);
    ~TrlObjects();

    class Item {
      public:
        Item() = default;
        Item(World&      world, const ParticleFx &ow);
        Item(TrlObjects& trl,   const ParticleFx &ow);
        Item(Item&& oth);
        Item& operator = (Item&& oth);
        ~Item();

        void setPosition(const Tempest::Vec3& pos);

      private:
        Bucket* bucket = nullptr;
        size_t  id     = 0;
      };

    void tick(uint64_t dt);
    void buildVbo(const Tempest::Vec3& viewDir);
    void preFrameUpdate(uint8_t fId);

  private:
    Bucket&           getBucket(const ParticleFx &ow);

    const SceneGlobals& scene;
    VisualObjects&      visual;
    std::list<Bucket>   bucket;
  };

