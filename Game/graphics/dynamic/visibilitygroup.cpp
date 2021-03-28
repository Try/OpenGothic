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
  t.pos = at;
  t.bbox.setObjMatrix(at);
  }

void VisibilityGroup::Token::setBounds(const Bounds& bbox) {
  if(owner==nullptr)
    return;
  auto& t = owner->tokens[id];
  t.bbox = bbox;
  t.bbox.setObjMatrix(owner->tokens[id].pos);
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

void VisibilityGroup::pass(const Tempest::Matrix4x4& main, const Tempest::Matrix4x4* sh, size_t /*shCount*/) {
  Frustrum f[V_Count];
  f[V_Shadow0].make(sh[0]);
  f[V_Shadow1].make(sh[1]);
  f[V_Main   ].make(main);

  Workers::parallelFor(tokens,[&f](Tok& t){
    auto& b = t.bbox;
    t.visible[V_Shadow0] = f[V_Shadow0].testPoint(b.midTr, b.r);
    t.visible[V_Shadow1] = f[V_Shadow1].testPoint(b.midTr, b.r);
    t.visible[V_Main]    = f[V_Main   ].testPoint(b.midTr, b.r);
    });
  }
