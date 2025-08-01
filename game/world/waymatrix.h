#pragma once

#include <Tempest/Painter>
#include <Tempest/Matrix4x4>

#include <zenkit/world/WayNet.hh>

#include <vector>
#include <functional>

#include "waypath.h"
#include "waypoint.h"

class World;
class DbgPainter;

class WayMatrix final {
  public:
    WayMatrix(World& owner, const zenkit::WayNet& dat);

    const WayPoint* findWayPoint (const Tempest::Vec3& at, const std::function<bool(const WayPoint&)>& filter) const;
    const WayPoint* findFreePoint(const Tempest::Vec3& at, std::string_view name, const std::function<bool(const WayPoint&)>& filter) const;
    const WayPoint* findNextPoint(const Tempest::Vec3& at) const;

    void            addFreePoint (const Tempest::Vec3& pos, const Tempest::Vec3& dir, std::string_view name);
    void            addStartPoint(const Tempest::Vec3& pos, const Tempest::Vec3& dir, std::string_view name);

    const WayPoint& startPoint() const;
    const WayPoint& deadPoint() const;
    void            buildIndex();

    const WayPoint* findPoint(std::string_view name, bool inexact) const;
    void            marchPoints(DbgPainter& p) const;

    WayPath         wayTo(const WayPoint** begin, size_t beginSz, const Tempest::Vec3 exactBegin, const WayPoint& end) const;

  private:
    World&                 world;
    // scripting doc says 20m, but number seems to be incorrect
    // Vatras requires at least 8 meters
    // Abuyin requires less than 10 meters
    // Gothic 1 range is identical
    float                  distanceThreshold = 900.f;

    std::vector<zenkit::WayEdge> edges;

    std::vector<WayPoint>  wayPoints;
    std::vector<WayPoint>  freePoints, startPoints;
    std::vector<WayPoint*> indexPoints;

    std::vector<WayPoint*> fpInd;

    struct FpIndex {
      std::string                  key;
      std::vector<const WayPoint*> index;
      };
    mutable std::vector<FpIndex>          fpIndex;

    mutable uint16_t                      pathGen=0;
    mutable std::vector<const WayPoint*>  stk[2];

    void                   adjustWaypoints(std::vector<WayPoint> &wp);
    void                   calculateLadderPoints();

    const FpIndex&         findFpIndex(std::string_view name) const;
    const WayPoint*        findFreePoint(float x, float y, float z, const FpIndex &ind,
                                         const std::function<bool(const WayPoint&)>& filter) const;
  };
