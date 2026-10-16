#include "arpgnav.h"

#include <algorithm>
#include <cmath>
#include <queue>


using namespace Tempest;

int ArpgNav::cellOf(float v) {
  return int(std::floor(v/Cell));
  }

float ArpgNav::center(int i) {
  return (float(i)+0.5f)*Cell;
  }

void ArpgNav::reset() {
  cells.clear();
  owner = nullptr;
  made  = 0;
  }

void ArpgNav::sync(const Scene& w) {
  // another world, or the cells are old (a door opened, something moved): test again
  if(owner!=w.id() || w.time()>made+4000) {
    cells.clear();
    owner = w.id();
    made  = w.time();
    }
  }

void ArpgNav::block(const Vec3& at) {
  auto& c = cells[key(cellOf(at.x), cellOf(at.z))];
  c.state = 2;
  }

const ArpgNav::CellInfo& ArpgNav::probe(const Scene& w, int i, int j, float nearY) {
  auto& c = cells[key(i,j)];
  if(c.state!=0)
    return c;
  const float x = center(i), z = center(j);

  // the ground: from a bit over the hero's level down (a table top counts as "ground" too: then it is too high)
  Vec3 g;
  if(!w.ray(Vec3(x, nearY+170.f, z), Vec3(x, nearY-260.f, z), g)) {
    c.state = 3;
    return c;
    }
  c.y = g.y;

  // the body: a sphere swept down from chest height to just over the ground; anything around (a table, a chair,
  // a crate, the edge of a shelf) stops it
  c.state = w.sweepDown(Vec3(x, c.y+150.f, z), Vec3(x, c.y+Radius+30.f, z), Radius) ? 2 : 1;
  return c;
  }

bool ArpgNav::link(const Scene& w, int i0, int j0, float y0, int i1, int j1, bool soft) {
  const auto& b = probe(w, i1, j1, y0);
  if(b.state==3 || (b.state==2 && !soft))
    return false;
  const bool diag = (i0!=i1 && j0!=j1);
  if(std::fabs(b.y-y0) > (diag ? Step*1.4f : Step))
    return false;
  if(diag && !soft) {
    // no cutting a corner of a table
    const auto& c1 = probe(w, i1, j0, y0);
    const auto& c2 = probe(w, i0, j1, y0);
    if(c1.state!=1 || c2.state!=1)
      return false;
    }
  // nothing between the two (thin walls, fences, the side of a shelf: the sphere of a cell does not see a wall it
  // starts in)
  const Vec3 a = {center(i0), y0+70.f,  center(j0)};
  const Vec3 e = {center(i1), b.y+70.f, center(j1)};
  Vec3       hit;
  return !w.ray(a, e, hit);
  }

bool ArpgNav::lineClear(const Scene& w, const Vec3& from, const Vec3& to) {
  sync(w);
  const float dx = to.x-from.x, dz = to.z-from.z;
  const float len = std::sqrt(dx*dx+dz*dz);
  if(len<1.f)
    return true;
  if(len>8000.f)
    return false; // very far: the waynet

  // the cells along the line and along two lines at the sides (the hero is not thin: a corner of a table may not come
  // closer than his room): free, the ground without jumps
  const float nx = -dz/len, nz = dx/len;   // to the side
  const int   n  = std::max(1, int(len/(Cell*0.5f)));
  float       y  = from.y;
  for(float off:{0.f, -12.f, 12.f}) {
    float yl = from.y;
    int   pi = cellOf(from.x+nx*off), pj = cellOf(from.z+nz*off);
    for(int s=1; s<=n; ++s) {
      const float t  = float(s)/float(n);
      const float px = from.x+dx*t+nx*off, pz = from.z+dz*t+nz*off;
      const int   ci = cellOf(px),  cj = cellOf(pz);
      if(ci==pi && cj==pj)
        continue;
      const auto& c = probe(w, ci, cj, yl);
      const bool  nearStart = (t*len < Radius+10.f); // the hero may stand close to something
      if(c.state==3 || (c.state==2 && !nearStart))
        return false;
      if(std::fabs(c.y-yl) > Step*1.4f)
        return false;
      yl = c.y;
      pi = ci;
      pj = cj;
      }
    if(off==0.f)
      y = yl;
    }

  // and nothing across the way at knee and chest height
  Vec3 hit;
  for(float h:{55.f, 120.f}) {
    if(w.ray(from+Vec3(0,h,0), Vec3(to.x, y+h, to.z), hit))
      return false;
    }
  return true;
  }

bool ArpgNav::findPath(const Scene& w, const Vec3& from, const Vec3& dest, float reach, std::vector<Vec3>& out) {
  sync(w);
  out.clear();
  const float fdx = dest.x-from.x, fdz = dest.z-from.z;
  if(fdx*fdx+fdz*fdz > 3000.f*3000.f || std::fabs(dest.y-from.y)>350.f)
    return false; // far or on another floor: the waynet

  reach = std::max(reach, Cell*0.75f);
  const int si = cellOf(from.x), sj = cellOf(from.z);
  const int di = cellOf(dest.x), dj = cellOf(dest.z);
  // the area searched: around the hero and the target, at most ~35 m
  const int margin = 16;
  const int i0 = std::min(si,di)-margin, i1 = std::max(si,di)+margin;
  const int j0 = std::min(sj,dj)-margin, j1 = std::max(sj,dj)+margin;
  if(i1-i0>140 || j1-j0>140)
    return false;

  struct Node {
    float   g      = 0;
    float   y      = 0;
    int64_t parent = 0;
    bool    closed = false;
    bool    root   = false;
    };
  std::unordered_map<int64_t, Node> nodes;
  struct Open {
    float   f, g;
    int     i, j;
    // the smallest f first; of equal ones the one further on (fewer cells searched on ways of the same length)
    bool operator < (const Open& o) const { return f!=o.f ? f>o.f : g<o.g; }
    };
  std::priority_queue<Open> open;

  auto h = [&](int i, int j) {
    // octile distance: exact for 8 directions on a free grid (straight ways need few cells searched)
    const float x  = std::fabs(center(i)-dest.x), z = std::fabs(center(j)-dest.z);
    const float mx = std::max(x,z), mn = std::min(x,z);
    // x1.002: of ways nearly as long the one further on goes first (float sums of equal ways are not quite equal)
    return std::max(0.f, (mx-mn) + mn*1.4142f - reach)*1.002f;
    };
  auto isGoal = [&](int i, int j) {
    const float x = center(i)-dest.x, z = center(j)-dest.z;
    return x*x+z*z <= reach*reach;
    };

  {
    Node& s = nodes[key(si,sj)];
    s.y    = from.y;
    s.root = true;
    open.push(Open{h(si,sj), 0.f, si, sj});
  }

  int     expanded = 0;
  int64_t goal     = 0;
  bool    found    = false;
  while(!open.empty() && expanded<1200) {
    const Open o = open.top();
    open.pop();
    const int64_t k = key(o.i, o.j);
    Node&         n = nodes[k];
    if(n.closed)
      continue;
    n.closed = true;
    ++expanded;
    if(isGoal(o.i, o.j) && !n.root) {
      goal  = k;
      found = true;
      break;
      }
    if(n.root && isGoal(o.i, o.j)) {
      out.push_back(dest);
      return true; // already there
      }
    const float g0 = n.g, y0 = n.y;
    const bool  soft = n.root; // the hero's own cell: he may stand close to something
    for(int dj=-1; dj<=1 && !found; ++dj) {
      for(int di2=-1; di2<=1; ++di2) {
        if(di2==0 && dj==0)
          continue;
        const int ni = o.i+di2, nj = o.j+dj;
        if(ni<i0 || ni>i1 || nj<j0 || nj>j1)
          continue;
        const int64_t nk = key(ni, nj);
        auto  it      = nodes.find(nk);
        const bool  existed = (it!=nodes.end());
        if(existed && it->second.closed)
          continue;
        const float oldG = existed ? it->second.g : 0.f;
        if(!link(w, o.i, o.j, y0, ni, nj, soft))
          continue;
        const float step = (di2!=0 && dj!=0) ? Cell*1.4142f : Cell;
        const float ng   = g0+step;
        if(existed && oldG<=ng)
          continue;
        Node& m = nodes[nk];
        m.g      = ng;
        m.y      = probe(w, ni, nj, y0).y;
        m.parent = k;
        if(isGoal(ni, nj)) {
          // close enough: done when it is found (not when it comes out of the queue - the last step is a whole
          // cell, more than the estimate, and the whole band around the way would be searched first)
          goal  = nk;
          found = true;
          break;
          }
        open.push(Open{ng+h(ni,nj), ng, ni, nj});
        }
      }
    if(found)
      break;
    }
  if(!found)
    return false;

  // the cells back from the goal
  std::vector<Vec3> raw;
  for(int64_t k=goal;;) {
    const Node& n = nodes[k];
    const int   i = int(k>>32);                 // key(): i in the high half,
    const int   j = int(int32_t(uint32_t(k)));  // j in the low one
    raw.push_back(Vec3(center(i), n.y, center(j)));
    if(n.root)
      break;
    k = n.parent;
    }
  std::reverse(raw.begin(), raw.end());
  if(raw.empty())
    return false;

  // only the corners: the farthest cell that can be walked to straight
  Vec3   at  = from;
  size_t idx = 1;
  while(idx<raw.size()) {
    size_t best = idx;
    for(size_t k=raw.size(); k>idx+1; --k) {
      if(lineClear(w, at, raw[k-1])) {
        best = k-1;
        break;
        }
      }
    out.push_back(raw[best]);
    at  = raw[best];
    idx = best+1;
    }
  // a click on the ground: right to the point when it can be stood on
  if(reach<=Cell*1.5f || lineClear(w, at, dest)) {
    const auto& dc = probe(w, di, dj, at.y);
    if(dc.state==1)
      out.push_back(dest);
    }
  return !out.empty();
  }
