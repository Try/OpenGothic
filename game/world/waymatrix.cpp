#include "waymatrix.h"

#include <Tempest/Log>
#include <algorithm>
#include <limits>

#include "utils/dbgpainter.h"
#include "utils/versioninfo.h"
#include "world/objects/interactive.h"
#include "world.h"

using namespace Tempest;

WayMatrix::WayMatrix(World &world, const phoenix::way_net &dat)
  :world(world) {
  // scripting doc says 20m, but number seems to be incorrect
  if(world.version().game==2) {
    // Vatras requires at least 8 meters
    // Abuyin requires less than 10 meters
    distanceThreshold = 9.f*100.f;
    }

  wayPoints.resize(dat.waypoints.size());
  for(size_t i=0;i<wayPoints.size();++i){
    wayPoints[i] = WayPoint(dat.waypoints[i]);
    }

  edges = dat.edges;

  for(auto& i:wayPoints)
    if(i.name.find("START")==0)
      startPoints.push_back(i);

  for(auto& i:wayPoints)
    if(i.name.find("START")!=std::string::npos)
      startPoints.push_back(i);

  stk[0].reserve(256);
  stk[1].reserve(256);
  }

void WayMatrix::buildIndex() {
  indexPoints.clear();
  adjustWaypoints(wayPoints);
  adjustWaypoints(freePoints);
  adjustWaypoints(startPoints);
  std::sort(indexPoints.begin(),indexPoints.end(),[](const WayPoint* a,const WayPoint* b){
    return a->name<b->name;
    });

  for(auto& i:freePoints)
    fpInd.push_back(&i);
  std::sort(fpInd.begin(),fpInd.end(),[](const WayPoint* a,const WayPoint* b){
    return a->x<b->x;
    });

  for(auto& i:edges) {
    if(i.a<wayPoints.size() && i.b<wayPoints.size()) {
      auto& a = wayPoints[i.a];
      auto& b = wayPoints[i.b];

      a.connect(b);
      b.connect(a);
      }
    }

  calculateLadderPoints();
  }

const WayPoint *WayMatrix::findWayPoint(const Vec3& at, const std::function<bool(const WayPoint&)>& filter) const {
  const WayPoint* ret =nullptr;
  float           dist=std::numeric_limits<float>::max();
  for(auto& w:wayPoints) {
    if(!filter(w))
      continue;
    auto  dp0 = at-w.position();
    float l0  = dp0.quadLength();

    if(l0<dist){
      ret  = &w;
      dist = l0;
      }
    }
  return ret;
  }

const WayPoint *WayMatrix::findFreePoint(const Vec3& at, std::string_view name, const std::function<bool(const WayPoint&)>& filter) const {
  auto&  index = findFpIndex(name);
  return findFreePoint(at.x,at.y,at.z,index,filter);
  }

const WayPoint *WayMatrix::findNextPoint(const Vec3& at) const {
  const WayPoint* ret   = nullptr;
  float           dist  = distanceThreshold;

  dist*=dist;
  for(auto pw:indexPoints){
    auto& w  = *pw;
    auto  dp = w.position()-at;
    float l  = dp.quadLength();

    if(l<dist && dp.z*dp.z<300*300 && !w.isLocked()){
      ret  = &w;
      dist = l;
      }
    }
  return ret;
  }

void WayMatrix::addFreePoint(const Vec3& pos, const Vec3& dir, std::string_view name) {
  freePoints.emplace_back(pos,dir,name);
  }

void WayMatrix::addStartPoint(const Vec3& pos, const Vec3& dir, std::string_view name) {
  startPoints.emplace_back(pos,dir,name);
  }

const WayPoint &WayMatrix::startPoint() const {
  for(auto& i:startPoints)
    if(i.name=="START_GOTHIC2")
      return i;

  for(auto& i:startPoints)
    if(i.name=="START")
      return i;

  if(startPoints.size()>0)
    return startPoints.back();
  static WayPoint p(Vec3(),"START");
  return p;
  }

const WayPoint& WayMatrix::deadPoint() const {
  for(auto& i:indexPoints)
    if(i->name=="TOT")
      return *i;
  static WayPoint p(Vec3(-1000,-1000,-1000),"TOT");
  return p;
  }

const WayPoint* WayMatrix::findPoint(std::string_view name, bool inexact) const {
  if(name.empty())
    return nullptr;
  for(auto& i:startPoints)
    if(name==i.name)
      return &i;
  auto it = std::lower_bound(indexPoints.begin(),indexPoints.end(),name,[](const WayPoint* a, std::string_view b){
      return a->name<b;
    });
  if(it!=indexPoints.end() && name==(*it)->name)
    return *it;
  if(!inexact)
    return nullptr;
  for(auto i:indexPoints)
    if(i->checkName(name))
      return i;
  return nullptr;
  }

void WayMatrix::marchPoints(DbgPainter &p) const {
  static bool ddraw=false;
  if(!ddraw)
    return;
  static bool fp = true;
  auto& points = fp ? freePoints : wayPoints;
  for(auto& i:points) {
    float x = i.x, y = i.y, z = i.z;
    p.mvp.project(x,y,z);
    if(z<0.f || z>1.f)
      continue;

    x = (0.5f*x+0.5f)*float(p.w);
    y = (0.5f*y+0.5f)*float(p.h);

    p.setBrush(Tempest::Color(1,0,0,1));
    p.painter.drawRect(int(x),int(y),4,4);

    p.drawText(int(x),int(y),i.name.c_str());
    }
  }

void WayMatrix::adjustWaypoints(std::vector<WayPoint> &wp) {
  for(auto& w:wp) {
    auto ray = world.physic()->landRay(w.position());
    if(ray.hasCol)
      w.y = ray.v.y;
    indexPoints.push_back(&w);
    }
  }

void WayMatrix::calculateLadderPoints() {
  static const float dist = 100.f;
  for(uint32_t i=0;;++i) {
    auto inter = world.mobsiById(i);
    if(inter==nullptr)
      break;
    if(!inter->isLadder())
      continue;
    auto box = inter->bBox();
    for(auto& e:edges) {
      if(e.a>=wayPoints.size() || e.b>=wayPoints.size() || e.a==e.b)
        continue;
      auto& a     = wayPoints[e.a], b    = wayPoints[e.b];
      Vec3  posA  = a.position(),   posB = b.position();
      Vec3  dTopA = box[1] - posA;
      Vec3  dBotA = box[0] - posA;
      Vec3  dBA   = posB - posA;
      if(dBA.x<0)
        std::swap(dTopA.x,dBotA.x);
      if(dBA.y<0)
        std::swap(dTopA.y,dBotA.y);
      if(dBA.z<0)
        std::swap(dTopA.z,dBotA.z);
      float max = std::min({dTopA.x/dBA.x,dTopA.y/dBA.y,dTopA.z/dBA.z});
      float min = std::max({dBotA.x/dBA.x,dBotA.y/dBA.y,dBotA.z/dBA.z});
      if(max<min || max<0.f || min>1.f)
        continue;
      float dy = 0.5f * std::abs(box[0].y+box[1].y-posA.y-posB.y);
      if(dy<dist) {
        wayPoints[e.a].ladder = inter;
        wayPoints[e.b].ladder = inter;
        break;
        }
      }
    }
  }

const WayMatrix::FpIndex &WayMatrix::findFpIndex(std::string_view name) const {
  auto it = std::lower_bound(fpIndex.begin(),fpIndex.end(),name,[](FpIndex& l, std::string_view r){
    return l.key<r;
    });
  if(it!=fpIndex.end() && it->key==name){
    return *it;
    }

  FpIndex id;
  id.key = name;
  for(auto& w:freePoints){
    if(!w.checkName(name))
      continue;
    id.index.push_back(&w);
    }
  // TODO: good index, not sort by 'x' :)
  std::sort(id.index.begin(),id.index.end(),[](const WayPoint* a,const WayPoint* b){
    return a->x<b->x;
    });

  it = fpIndex.insert(it,std::move(id));
  return *it;
  }

const WayPoint *WayMatrix::findFreePoint(float x, float y, float z, const FpIndex& ind,
                                         const std::function<bool(const WayPoint&)>& filter) const {
  float R = distanceThreshold;
  auto b = std::lower_bound(ind.index.begin(),ind.index.end(), x-R ,[](const WayPoint *a, float b){
    return a->x<b;
    });
  auto e = std::upper_bound(ind.index.begin(),ind.index.end(), x+R ,[](float a,const WayPoint *b){
    return a<b->x;
    });

  const WayPoint* ret=nullptr;
  auto  count = std::distance(b,e);(void) count;
  float dist  = R*R;
  for(auto i=b;i!=e;++i){
    auto& w  = **i;
    float dx = w.x-x;
    float dy = w.y-y;
    float dz = w.z-z;
    float l  = dx*dx+dy*dy+dz*dz;
    if(l>dist || dz*dz>300*300)
      continue;
    if(!filter(w))
      continue;
    ret  = &w;
    dist = l;
    }
  return ret;
  }

WayPath WayMatrix::wayTo(const WayPoint** begin, size_t beginSz, const Tempest::Vec3 exactBegin, const WayPoint& end) const {
  if(beginSz==0)
    return WayPath();

  intptr_t endId = std::distance<const WayPoint*>(&wayPoints[0],&end);
  if(endId<0 || size_t(endId)>=wayPoints.size()){
    if(end.name.find("FP_")==0) {
      WayPath ret;
      ret.add(end);
      return ret;
      }
    return WayPath();
    }

  pathGen++;
  if(pathGen==1){
    // new cycle
    for(auto& i:wayPoints)
      i.pathGen=0;
    }

  std::vector<const WayPoint*> *front=&stk[0], *back=&stk[1];
  stk[0].clear();
  stk[1].clear();

  end.pathLen = 0;
  end.pathGen = pathGen;
  front->push_back(&end);

  while(front->size()>0) {
    bool done = true;
    for(size_t i=0; i<beginSz; ++i)
      if(begin[i]->pathGen!=pathGen) {
        done = false;
        break;
        }
    if(done)
      break;

    for(auto& wp:*front) {
      int32_t l0 = wp->pathLen;

      for(auto i:wp->connections()){
        auto& w  = *i.point;
        int32_t l1 = l0+i.len;
        if(w.pathGen!=pathGen || w.pathLen>l1){
          w.pathLen = l1;
          w.pathGen = pathGen;
          back->push_back(&w);
          }
        }
      }
    std::swap(front,back);
    back->clear();
    }

  const WayPoint* first = begin[0];
  for(size_t i=0; i<beginSz; ++i) {
    int32_t iLen = begin[i]->pathLen + int((exactBegin - begin[i]->position()).length());
    int32_t fLen = first   ->pathLen + int((exactBegin - first   ->position()).length());
    if(iLen<fLen)
      first = begin[i];
    }

  WayPath ret;
  ret.add(*first);
  const WayPoint* current = first;
  while(current!=&end) {
    int32_t l0 = current->pathLen, l1=l0;

    const WayPoint* next=nullptr;
    for(auto i:current->connections()){
      if(i.point->pathGen==pathGen && i.point->pathLen+i.len<=l0 && i.point->pathLen<l1){
        next = i.point;
        l1   = i.point->pathLen;
        }
      }
    if(next==nullptr)
      return WayPath();
    ret.add(*next);
    current=next;
    }

  ret.reverse();
  return ret;
  }
