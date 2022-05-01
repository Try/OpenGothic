#include "visibilitygroup.h"

#include "frustrum.h"
#include "visibleset.h"
#include "utils/workers.h"

using namespace Tempest;

VisibilityGroup::Token::Token(VisibilityGroup& owner, TokList& group, size_t id)
  :owner(&owner), group(&group), id(id) {
  }

VisibilityGroup::Token::Token(VisibilityGroup::Token&& other)
  :owner(other.owner),group(other.group),id(other.id) {
  other.owner = nullptr;
  other.group = nullptr;
  }

VisibilityGroup::Token& VisibilityGroup::Token::operator =(VisibilityGroup::Token&& t) {
  std::swap(owner,t.owner);
  std::swap(group,t.group);
  std::swap(id,   t.id);
  return *this;
  }

VisibilityGroup::Token::~Token() {
  if(group==nullptr)
    return;
  auto& t = group->tokens[id];
  t.vSet = nullptr;
  group->freeList.push_back(id);
  }

void VisibilityGroup::Token::setObject(VisibleSet* b, size_t i) {
  auto& t = group->tokens[id];
  t.vSet = b;
  t.id   = i;
  }

void VisibilityGroup::Token::setObjMatrix(const Matrix4x4& at) {
  if(group==nullptr)
    return;
  auto& t = group->tokens[id];
  t.pos        = at;
  t.updateBbox = true;
  }

void VisibilityGroup::Token::setGroup(Group gr) {
  if(owner==nullptr)
    return;
  auto& g = owner->group(gr);
  if(&g==group)
    return;

  auto prevId = id;
  if(g.freeList.size()>0) {
    size_t id2 = g.freeList.back();
    g.freeList.pop_back();
    g.tokens[id2] = group->tokens[id];
    id = id2;
    } else {
    g.tokens.push_back(group->tokens[id]);
    id = g.tokens.size()-1;
    }
  group->tokens[prevId] = Tok();
  group->freeList.push_back(prevId);
  group = &g;
  }

void VisibilityGroup::Token::setBounds(const Bounds& bbox) {
  if(group==nullptr)
    return;
  auto& t = group->tokens[id];
  t.bbox       = bbox;
  t.updateBbox = true;
  }

const Bounds& VisibilityGroup::Token::bounds() const {
  return group->tokens[id].bbox;
  }

VisibilityGroup::VisibilityGroup(const std::pair<Vec3, Vec3>& bbox) {
  def.freeList.reserve(4);
  }

VisibilityGroup::Token VisibilityGroup::get(Group g) {
  auto& gr = group(g);
  size_t id = gr.tokens.size();
  if(gr.freeList.size()>0) {
    id = gr.freeList.back();
    gr.freeList.pop_back();
    } else {
    gr.tokens.emplace_back();
    }
  return Token(*this, gr,id);
  }

void VisibilityGroup::pass(const Frustrum f[]) {
  float mX   = 2.f/float(f[SceneGlobals::V_Main].width );
  float mY   = 2.f/float(f[SceneGlobals::V_Main].height);

  float sh1X = 2.f/float(f[SceneGlobals::V_Shadow1].width );
  float sh1Y = 2.f/float(f[SceneGlobals::V_Shadow1].height);

  for(auto& t:alwaysVis.tokens) {
    if(t.vSet==nullptr)
      continue;
    t.vSet->push(t.id,SceneGlobals::V_Shadow0);
    t.vSet->push(t.id,SceneGlobals::V_Shadow1);
    t.vSet->push(t.id,SceneGlobals::V_Main);
    }

  Workers::parallelFor(def.tokens,[&f,sh1X,sh1Y,mX,mY](Tok& t) {
    auto& b = t.bbox;
    if(t.vSet==nullptr)
      return;
    if(t.updateBbox) {
      t.bbox.setObjMatrix(t.pos);
      t.updateBbox = false;
      }

    bool  visible[SceneGlobals::V_Count] = {};
    float dist   [SceneGlobals::V_Count] = {};
    visible[SceneGlobals::V_Shadow0] = f[SceneGlobals::V_Shadow0].testPoint(b.midTr, b.r, dist[SceneGlobals::V_Shadow0]);
    visible[SceneGlobals::V_Shadow1] = f[SceneGlobals::V_Shadow1].testPoint(b.midTr, b.r, dist[SceneGlobals::V_Shadow1]);
    visible[SceneGlobals::V_Main]    = f[SceneGlobals::V_Main   ].testPoint(b.midTr, b.r, dist[SceneGlobals::V_Main   ]);

    if(b.r<1.f*100.f) {
      static const float farDist = 50*100; // 50 meters
      if(visible[SceneGlobals::V_Shadow1] && dist[SceneGlobals::V_Shadow1]>b.r+farDist)
        visible[SceneGlobals::V_Shadow1] = subpixelMeshTest(t,f[SceneGlobals::V_Shadow1],sh1X,sh1Y);
      if(visible[SceneGlobals::V_Main] && dist[SceneGlobals::V_Main]>b.r+farDist)
        visible[SceneGlobals::V_Main] = subpixelMeshTest(t,f[SceneGlobals::V_Main],mX,mY);
      }

    if(visible[SceneGlobals::V_Shadow0])
      t.vSet->push(t.id,SceneGlobals::V_Shadow0);
    if(visible[SceneGlobals::V_Shadow1])
      t.vSet->push(t.id,SceneGlobals::V_Shadow1);
    if(visible[SceneGlobals::V_Main])
      t.vSet->push(t.id,SceneGlobals::V_Main);
    });
  }

VisibilityGroup::TokList& VisibilityGroup::group(Group gr) {
  switch(gr) {
    case G_Default:   return def;
    case G_Static:    return def;
    case G_AlwaysVis: return alwaysVis;
    }
  return def;
  }

bool VisibilityGroup::subpixelMeshTest(const Tok& t, const Frustrum& f, float edgeX, float edgeY) {
  auto& b = t.bbox.bbox;
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

  for(auto& i:pt) {
    t.pos.project(i);
    f.mat.project(i);
    }

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
