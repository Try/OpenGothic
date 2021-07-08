#include "visibilitygroup.h"

#include "frustrum.h"
#include "visibleset.h"
#include "utils/workers.h"

using namespace Tempest;

VisibilityGroup::Token::Token(VisibilityGroup& ow, size_t id)
  :owner(&ow), id(id) {
  }

VisibilityGroup::Token::Token(VisibilityGroup::Token&& other)
  :owner(other.owner),id(other.id) {
  other.owner = nullptr;
  }

VisibilityGroup::Token& VisibilityGroup::Token::operator =(VisibilityGroup::Token&& t) {
  std::swap(owner,t.owner);
  std::swap(id,   t.id);
  return *this;
  }

VisibilityGroup::Token::~Token() {
  if(owner==nullptr)
    return;
  auto& t = owner->tokens[id];
  t.vSet = nullptr;
  owner->freeList.push_back(id);
  }

void VisibilityGroup::Token::setObject(VisibleSet* b, size_t i) {
  auto& t = owner->tokens[id];
  t.vSet = b;
  t.id   = i;
  }

void VisibilityGroup::Token::setObjMatrix(const Matrix4x4& at) {
  if(owner==nullptr)
    return;
  auto& t = owner->tokens[id];
  t.pos        = at;
  t.updateBbox = true;
  }

void VisibilityGroup::Token::setAlwaysVis(bool v) {
  auto& t = owner->tokens[id];
  t.alwaysVis = v;
  }

void VisibilityGroup::Token::setAsOccluder(bool v) {
  auto& t = owner->tokens[id];
  t.occluder = v;
  }

void VisibilityGroup::Token::setBounds(const Bounds& bbox) {
  if(owner==nullptr)
    return;
  auto& t = owner->tokens[id];
  t.bbox       = bbox;
  t.updateBbox = true;
  }

const Bounds& VisibilityGroup::Token::bounds() const {
  return owner->tokens[id].bbox;
  }

VisibilityGroup::VisibilityGroup() {
  freeList.reserve(4);
  }

VisibilityGroup::Token VisibilityGroup::get() {
  size_t id = tokens.size();
  if(freeList.size()>0) {
    id = freeList.back();
    freeList.pop_back();
    } else {
    tokens.emplace_back();
    }
  // auto& t = tokens[id];
  // t.pos  = at;
  // t.bbox = bbox;
  // t.bbox.setObjMatrix(at);
  return Token(*this,id);
  }

void VisibilityGroup::pass(const Frustrum f[], const Tempest::Pixmap& hiZ) {
  // float mX   = 2.f/float(f[SceneGlobals::V_Main].width );
  // float mY   = 2.f/float(f[SceneGlobals::V_Main].height);

  float sh1X = 2.f/float(f[SceneGlobals::V_Shadow1].width );
  float sh1Y = 2.f/float(f[SceneGlobals::V_Shadow1].height);

  Workers::parallelFor(tokens,[&f,&hiZ,sh1X,sh1Y](Tok& t) {
    auto& b = t.bbox;
    if(t.vSet==nullptr)
      return;
    if(t.updateBbox) {
      t.bbox.setObjMatrix(t.pos);
      t.updateBbox = false;
      }

    if(t.alwaysVis) {
      t.vSet->push(t.id,SceneGlobals::V_Shadow0);
      t.vSet->push(t.id,SceneGlobals::V_Shadow1);
      t.vSet->push(t.id,SceneGlobals::V_Main);
      } else {
      bool visible[SceneGlobals::V_Count] = {};
      visible[SceneGlobals::V_Shadow0] = f[SceneGlobals::V_Shadow0].testPoint(b.midTr, b.r);
      visible[SceneGlobals::V_Shadow1] = f[SceneGlobals::V_Shadow1].testPoint(b.midTr, b.r);
      visible[SceneGlobals::V_Main]    = f[SceneGlobals::V_Main   ].testPoint(b.midTr, b.r);

      if(visible[SceneGlobals::V_Shadow1])
        visible[SceneGlobals::V_Shadow1] = subpixelMeshTest(t,f[SceneGlobals::V_Shadow1],sh1X,sh1Y);
      if(visible[SceneGlobals::V_Main] && !t.occluder)
        visible[SceneGlobals::V_Main] = subpixelMeshTest(t,f[SceneGlobals::V_Main],hiZ);

      if(visible[SceneGlobals::V_Shadow0])
        t.vSet->push(t.id,SceneGlobals::V_Shadow0);
      if(visible[SceneGlobals::V_Shadow1])
        t.vSet->push(t.id,SceneGlobals::V_Shadow1);
      if(visible[SceneGlobals::V_Main])
        t.vSet->push(t.id,SceneGlobals::V_Main);
      }
    });
  }

bool VisibilityGroup::subpixelMeshTest(const Tok& t, const Frustrum& f, float edgeX, float edgeY) {
  auto& b = t.bbox.bboxTr;
  Vec3 pt[8] = {
    {b[0].x,b[0].y,b[0].z},
    {b[1].x,b[0].y,b[0].z},
    {b[1].x,b[1].y,b[0].z},
    {b[0].x,b[1].y,b[0].z},

    {b[0].x,b[0].y,b[1].z},
    {b[1].x,b[0].y,b[1].z},
    {b[1].x,b[1].y,b[1].z},
    {b[0].x,b[1].y,b[1].z},
    };

  for(auto& i:pt)
    f.mat.project(i.x,i.y,i.z);
  Vec2 bboxSh[2] = {};
  bboxSh[0].x = pt[0].x;
  bboxSh[0].y = pt[0].y;
  bboxSh[1] = bboxSh[0];
  for(auto& i:pt) {
    bboxSh[0].x = std::min(bboxSh[0].x,i.x);
    bboxSh[0].y = std::min(bboxSh[0].y,i.y);
    bboxSh[1].x = std::max(bboxSh[1].x,i.x);
    bboxSh[1].y = std::max(bboxSh[1].y,i.y);
    }
  float w = (bboxSh[1].x-bboxSh[0].x);
  float h = (bboxSh[1].y-bboxSh[0].y);
  return (w>edgeX && h>edgeY);
  }

bool VisibilityGroup::subpixelMeshTest(const Tok& t, const Frustrum& f, const Tempest::Pixmap& hiZ) {
  auto& b = t.bbox.bboxTr;
  Vec3 pt[8] = {
    {b[0].x,b[0].y,b[0].z},
    {b[1].x,b[0].y,b[0].z},
    {b[1].x,b[1].y,b[0].z},
    {b[0].x,b[1].y,b[0].z},

    {b[0].x,b[0].y,b[1].z},
    {b[1].x,b[0].y,b[1].z},
    {b[1].x,b[1].y,b[1].z},
    {b[0].x,b[1].y,b[1].z},
    };

  for(auto& i:pt)
    f.mat.project(i.x,i.y,i.z);
  Vec2 bboxSh[2] = {};
  bboxSh[0].x = pt[0].x;
  bboxSh[0].y = pt[0].y;
  bboxSh[1] = bboxSh[0];
  float bZ = pt[0].z;
  for(auto& i:pt) {
    bboxSh[0].x = std::min(bboxSh[0].x,i.x);
    bboxSh[0].y = std::min(bboxSh[0].y,i.y);
    bboxSh[1].x = std::max(bboxSh[1].x,i.x);
    bboxSh[1].y = std::max(bboxSh[1].y,i.y);
    bZ = std::min(bZ,pt[0].z);
    }

  if(bZ<0.25)
    return true;

  const int w = hiZ.w();
  const int h = hiZ.h();

  Point szI[2] = {};
  szI[0].x = int( float(w)*(bboxSh[0].x*0.5f+0.5f) );
  szI[0].y = int( float(h)*(bboxSh[0].y*0.5f+0.5f) );
  szI[1].x = int( std::ceil(float(w)*(bboxSh[1].x*0.5f+0.5f)) );
  szI[1].y = int( std::ceil(float(h)*(bboxSh[1].y*0.5f+0.5f)) );

  for(size_t i=0; i<2; ++i) {
    szI[i].x = std::max(0,std::min(szI[i].x,w-1));
    szI[i].y = std::max(0,std::min(szI[i].y,h-1));
    }

  if(szI[0].x+4<szI[1].x || szI[0].y+4<szI[1].y )
    return true;

  for(int i=szI[0].x; i<=szI[1].x; ++i)
    for(int r=szI[0].y; r<=szI[1].y; ++r) {
      float z = reinterpret_cast<const float*>(hiZ.data())[szI[0].x+szI[0].y*w];
      if(bZ<=z)
        return true;
      }
  return false;
  }
