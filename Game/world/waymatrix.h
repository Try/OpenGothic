#pragma once

#include <Tempest/Painter>
#include <Tempest/Matrix4x4>

#include <zenload/zTypes.h>
#include <vector>

#include "waypoint.h"

class World;

class WayMatrix final {
  public:
    WayMatrix(World& owner,const ZenLoad::zCWayNetData& dat);

    const WayPoint* findWayPoint (float x,float y,float z) const;
    const WayPoint* findFreePoint(float x,float y,float z,const char* name) const;
    const WayPoint* findNextFreePoint(float x,float y,float z,const char* name) const;
    const WayPoint* findNextPoint(float x,float y,float z) const;

    void            addFreePoint (float x,float y,float z,const char* name);
    void            addStartPoint(float x,float y,float z,const char* name);

    const WayPoint& startPoint() const;
    void            buildIndex();

    const WayPoint* findPoint(const char* name) const;
    void            marchPoints(Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

  private:
    World&                 world;

    std::vector<WayPoint>  wayPoints;
    std::vector<WayPoint>  freePoints, startPoints;
    std::vector<WayPoint*> indexPoints;

    struct FpIndex {
      std::string                  key;
      std::vector<const WayPoint*> index;
      };
    mutable std::vector<FpIndex>          fpIndex;

    void                   adjustWaypoints(std::vector<WayPoint> &wp);

    const FpIndex&         findFpIndex(const char* name) const;
  };
