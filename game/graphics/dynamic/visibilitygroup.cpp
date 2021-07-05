#include "frustrum.h"
#include "visibilitygroup.h"

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
  owner->freeList.push_back(id);
  }

void VisibilityGroup::Token::setObjMatrix(const Matrix4x4& at) {
  if(owner==nullptr)
    return;
  auto& t = owner->tokens[id];
  t.pos        = at;
  t.updateBbox = true;
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

void VisibilityGroup::pass(const Frustrum f[]) {
  float mX   = 2.f/float(f[SceneGlobals::V_Main].width );
  float mY   = 2.f/float(f[SceneGlobals::V_Main].height);

  float sh1X = 2.f/float(f[SceneGlobals::V_Shadow1].width );
  float sh1Y = 2.f/float(f[SceneGlobals::V_Shadow1].height);

  Workers::parallelFor(tokens,[&f,sh1X,sh1Y,mX,mY](Tok& t) {
    auto& b = t.bbox;
    if(t.updateBbox) {
      t.bbox.setObjMatrix(t.pos);
      t.updateBbox = false;
      }
    t.visible[SceneGlobals::V_Shadow0] = f[SceneGlobals::V_Shadow0].testPoint(b.midTr, b.r);
    t.visible[SceneGlobals::V_Shadow1] = f[SceneGlobals::V_Shadow1].testPoint(b.midTr, b.r);
    t.visible[SceneGlobals::V_Main]    = f[SceneGlobals::V_Main   ].testPoint(b.midTr, b.r);

    if(t.visible[SceneGlobals::V_Shadow1])
      t.visible[SceneGlobals::V_Shadow1] = subpixelMeshTest(t,f[SceneGlobals::V_Shadow1],sh1X,sh1Y);
    if(t.visible[SceneGlobals::V_Main])
      t.visible[SceneGlobals::V_Main] = subpixelMeshTest(t,f[SceneGlobals::V_Main],mX,mY);
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
