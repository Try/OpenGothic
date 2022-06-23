#include "visibilitygroup.h"

#include <Tempest/Log>

#include "frustrum.h"
#include "visibleset.h"
#include "utils/workers.h"

#include "graphics/objectsbucket.h"

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
  if(group==&owner->stat)
    owner->updateThree = true;
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
  if(group==&owner->stat)
    owner->updateThree = true;
  }

void VisibilityGroup::Token::setGroup(Group gr) {
  if(owner==nullptr)
    return;
  auto& g = owner->group(gr);
  if(&g==group)
    return;

  const auto prevId = id;
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
  if(group==&owner->stat)
    owner->updateThree = true;
  }

const Bounds& VisibilityGroup::Token::bounds() const {
  auto& t = group->tokens[id];
  if(t.updateBbox) {
    t.bbox.setObjMatrix(t.pos);
    t.updateBbox = false;
    }
  return t.bbox;
  }

VisibilityGroup::VisibilityGroup(const std::pair<Vec3, Vec3>& bbox) {
  def .freeList.reserve(4);
  stat.freeList.reserve(4);
  }

VisibilityGroup::TokList& VisibilityGroup::group(Group gr) {
  switch(gr) {
    case G_Default:   return def;
    case G_Static:    return stat;
    case G_AlwaysVis: return alwaysVis;
    }
  return def;
  }

void VisibilityGroup::buildTree() {
  treeTok.resize(stat.tokens.size());

  size_t tSz = 0;
  for(auto& t:stat.tokens) {
    if(t.vSet==nullptr)
      continue;
    auto& b = t.bbox.bbox;
    Vec3 pt[8] = {
      {b[0].x,b[0].y,b[0].z},
      {b[1].x,b[0].y,b[0].z},
      {b[0].x,b[1].y,b[0].z},
      {b[1].x,b[1].y,b[0].z},

      {b[0].x,b[0].y,b[1].z},
      {b[1].x,b[0].y,b[1].z},
      {b[0].x,b[1].y,b[1].z},
      {b[1].x,b[1].y,b[1].z},
      };
    for(auto& i:pt)
      t.pos.project(i);

    auto& tx = treeTok[tSz];
    tx.self    = &t;
    tx.bbox[0] = pt[0];
    tx.bbox[1] = pt[1];
    for(auto& i:pt) {
      tx.bbox[0].x = std::min(tx.bbox[0].x, i.x);
      tx.bbox[0].y = std::min(tx.bbox[0].y, i.y);
      tx.bbox[0].z = std::min(tx.bbox[0].z, i.z);
      tx.bbox[1].x = std::max(tx.bbox[1].x, i.x);
      tx.bbox[1].y = std::max(tx.bbox[1].y, i.y);
      tx.bbox[1].z = std::max(tx.bbox[1].z, i.z);
      }
    tx.midTr = (tx.bbox[1]+tx.bbox[0])*0.5f;
    ++tSz;
    }
  treeTok .resize(tSz);
  treeNode.resize(tSz);
  treeNode.resize(2); // dummy node + root
  buildTree(1,treeTok.data(),treeTok.data()+treeTok.size(),0);

  uint8_t maxTh = Workers::maxThreads();
  size_t  depth = 1;

  if(maxTh>=8)
    depth = 3;
  else if(maxTh>=4)
    depth = 2;
  else if(maxTh>=2)
    depth = 1;
  else if(maxTh>=0)
    depth = 0;
  treeTasks.clear();
  buildTreeTasks(1,depth,treeTok.data(),treeTok.data()+treeTok.size());
  }

void VisibilityGroup::buildTree(size_t node, TreeItm* begin, TreeItm* end, size_t step) {
  size_t sz = size_t(std::distance(begin,end));

  Vec3 bbox[2] = {};
  for(auto i=begin; i!=end; ++i) {
    if(bbox[0]==bbox[1]) {
      bbox[0] = i->bbox[0];
      bbox[1] = i->bbox[1];
      }
    bbox[0].x = std::min(bbox[0].x, i->bbox[0].x);
    bbox[0].y = std::min(bbox[0].y, i->bbox[0].y);
    bbox[0].z = std::min(bbox[0].z, i->bbox[0].z);

    bbox[1].x = std::max(bbox[1].x, i->bbox[1].x);
    bbox[1].y = std::max(bbox[1].y, i->bbox[1].y);
    bbox[1].z = std::max(bbox[1].z, i->bbox[1].z);
    }

  Node n;
  n.bbox.assign(bbox);
  treeNode[node] = n;

  const float blockSz = 5*100;
  const auto  boxSz   = bbox[1]-bbox[0];
  if(sz<16 || (boxSz.x<blockSz && boxSz.y<blockSz && boxSz.z<blockSz)) {
    treeNode[node].isLeaf = true;
    return;
    }

  if(step==0) {
    std::sort(begin,end,[](const TreeItm& l, const TreeItm& r) {
      return l.self->bbox.r < r.self->bbox.r;
      });
    }
  else switch(step%3) {
    case 0:{
      std::sort(begin,end,[](const TreeItm& l, const TreeItm& r) {
        return l.midTr.x < r.midTr.x;
        });
      break;
      }
    case 1:{
      std::sort(begin,end,[](const TreeItm& l, const TreeItm& r) {
        return l.midTr.y < r.midTr.y;
        });
      break;
      }
    case 2:{
      std::sort(begin,end,[](const TreeItm& l, const TreeItm& r) {
        return l.midTr.z < r.midTr.z;
        });
      break;
      }
    }

  treeNode.resize(std::max(treeNode.size(),node*2+2));
  buildTree(node*2+0, begin,      begin+sz/2, step+1);
  buildTree(node*2+1, begin+sz/2, begin+sz,   step+1);
  }

void VisibilityGroup::buildTreeTasks(size_t node, size_t depth, TreeItm* begin, TreeItm* end) {
  if(treeNode.size()<=node)
    return;
  auto& n = treeNode[node];
  if(n.isLeaf || depth==0) {
    TreeTask task = {};
    task.begin = begin;
    task.end   = end;
    task.node  = node;
    treeTasks.push_back(task);
    return;
    }
  auto sz = size_t(std::distance(begin,end));
  buildTreeTasks(node*2+0, depth-1, begin,      begin+sz/2);
  buildTreeTasks(node*2+1, depth-1, begin+sz/2, begin+sz  );
  }

VisibilityGroup::Token VisibilityGroup::get(Group g) {
  auto&  gr = group(g);
  size_t id = gr.tokens.size();
  if(gr.freeList.size()>0) {
    id = gr.freeList.back();
    gr.freeList.pop_back();
    } else {
    gr.tokens.emplace_back();
    }
  if(&gr==&stat) {
    updateThree = true;
    }
  return Token(*this, gr,id);
  }

void VisibilityGroup::pass(const Frustrum f[]) {
  if(updateThree) {
    buildTree();
    updateThree = false;
    }

  Workers::parallelFor(resetableSets,[](VisibleSet *v){
    v->reset();
    });

  for(auto& t:alwaysVis.tokens) {
    if(t.vSet==nullptr)
      continue;
    t.vSet->push(t.id,SceneGlobals::V_Shadow0);
    t.vSet->push(t.id,SceneGlobals::V_Shadow1);
    t.vSet->push(t.id,SceneGlobals::V_Main);
    }

  testStaticObjectsThreaded(f);

  Workers::parallelFor(def.tokens,[&f](Tok& t) {
    testVisibility(t,f);
    });
  }

void VisibilityGroup::buildVSetIndex(const std::vector<ObjectsBucket*>& index) {
  resetableSets.reserve(index.size());
  resetableSets.clear();

  for(auto i:index) {
    auto& v = i->visibilitySet();
    resetableSets.push_back(&v);
    }
  std::sort(resetableSets.begin(),resetableSets.end());
  }

void VisibilityGroup::testStaticObjectsThreaded(const Frustrum f[]) {
  Workers::parallelTasks(treeTasks.size(),[&](uintptr_t taskId) {
    auto& t = treeTasks[taskId];
    for(uint8_t c=SceneGlobals::V_Shadow0; c<SceneGlobals::V_Count; ++c)
      testStaticObjects(f,SceneGlobals::VisCamera(c),t.node, t.begin,t.end);
    });
  }

void VisibilityGroup::setVisible(SceneGlobals::VisCamera c, TreeItm* begin, TreeItm* end) {
  for(auto i=begin; i!=end; ++i) {
    auto& v = *i->self->vSet;
    v.push(i->self->id, c);
    }
  }

void VisibilityGroup::testStaticObjects(const Frustrum f[], SceneGlobals::VisCamera c,
                                        size_t node, TreeItm* begin, TreeItm* end) {
  if(treeNode.size()<=node)
    return;
  auto& n       = treeNode[node];
  auto  visible = f[c].testBbox(n.bbox.bbox[0],n.bbox.bbox[1]);

  if(n.isLeaf || visible==Frustrum::T_Full) {
    setVisible(SceneGlobals::VisCamera(c),begin,end);
    return;
    }
  if(visible==Frustrum::T_Invisible) {
    return;
    }

  size_t sz = size_t(std::distance(begin,end));
  testStaticObjects(f,c, node*2+0, begin,     begin+sz/2);
  testStaticObjects(f,c, node*2+1, begin+sz/2,end);
  }

void VisibilityGroup::testVisibility(Tok& t, const Frustrum f[]) {
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

  /*
  if(b.r<1.f*100.f) {
    static const float farDist = 50*100; // 50 meters
    if(visible[SceneGlobals::V_Shadow1] && dist[SceneGlobals::V_Shadow1]>b.r+farDist)
      visible[SceneGlobals::V_Shadow1] = subpixelMeshTest(t,f[SceneGlobals::V_Shadow1],sh1X,sh1Y);
    if(visible[SceneGlobals::V_Main] && dist[SceneGlobals::V_Main]>b.r+farDist)
      visible[SceneGlobals::V_Main] = subpixelMeshTest(t,f[SceneGlobals::V_Main],mX,mY);
    }*/

  if(visible[SceneGlobals::V_Shadow0])
    t.vSet->push(t.id,SceneGlobals::V_Shadow0);
  if(visible[SceneGlobals::V_Shadow1])
    t.vSet->push(t.id,SceneGlobals::V_Shadow1);
  if(visible[SceneGlobals::V_Main])
    t.vSet->push(t.id,SceneGlobals::V_Main);
  }

bool VisibilityGroup::subpixelMeshTest(const Tok& t, const Frustrum& f, float edgeX, float edgeY) {
  auto& b = t.bbox.bbox;
  Vec3 pt[8] = {
    {b[0].x,b[0].y,b[0].z},
    {b[1].x,b[0].y,b[0].z},
    {b[0].x,b[1].y,b[0].z},
    {b[1].x,b[1].y,b[0].z},

    {b[0].x,b[0].y,b[1].z},
    {b[1].x,b[0].y,b[1].z},
    {b[0].x,b[1].y,b[1].z},
    {b[1].x,b[1].y,b[1].z},
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
