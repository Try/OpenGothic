#include "spaceindex.h"

#include "world/objects/vob.h"

void BaseSpaceIndex::clear() {
  arr.clear();
  index.clear();
  dynamic.clear();
  }

void BaseSpaceIndex::invalidate() {
  index.clear();
  dynamic.clear();
  }

void BaseSpaceIndex::add(Vob* v) {
  arr.push_back(v);
  index.reserve(arr.size());
  index.clear();
  dynamic.clear();
  }

void BaseSpaceIndex::del(Vob* v) {
  for(size_t i=0;i<arr.size();++i) {
    if(arr[i]==v) {
      arr[i] = arr.back();
      arr.pop_back();
      index.clear();
      dynamic.clear();
      return;
      }
    }
  }

bool BaseSpaceIndex::hasObject(const Vob* v) const {
  if(v==nullptr)
    return false;
  for(size_t i=0;i<arr.size();++i)
    if(arr[i]==v)
      return true;
  return false;
  }

void BaseSpaceIndex::find(const Tempest::Vec3& p, float R, const void* ctx, void (*func)(const void*, Vob*)) {
  if(index.size()==0)
    buildIndex();
  for(auto& i:dynamic)
    (*func)(ctx,i);
  implFind(index.data(),index.size(),0,p,R,ctx,func);
  }

void BaseSpaceIndex::buildIndex() {
  size_t cnt = 0;
  index.resize(arr.size());
  dynamic.clear();
  for(size_t i=0; i<arr.size(); ++i) {
    if(arr[i]->isDynamic()) {
      dynamic.push_back(arr[i]);
      } else {
      index[cnt] = arr[i];
      ++cnt;
      }
    }
  index.resize(cnt);
  buildIndex(index.data(),index.size(),0);
  }

void BaseSpaceIndex::buildIndex(Vob** v, size_t cnt, uint8_t depth) {
  depth%=3;
  sort(v,cnt,depth);
  size_t mid = cnt/2;
  if(mid>0) {
    // [0..mid)
    buildIndex(v,mid,uint8_t(depth+1u));
    }
  if(mid+1<cnt) {
    // (mid..cnt)
    buildIndex(v+mid+1,cnt-mid-1,uint8_t(depth+1u));
    }
  }

void BaseSpaceIndex::sort(Vob** v, size_t cnt, uint8_t component) {
  bool (*predicate)(const Vob* a, const Vob* b) = nullptr;
  switch(component) {
    case 0:
      predicate = [](const Vob* a, const Vob* b){ return a->position().x < b->position().x; };
      break;
    case 1:
      predicate = [](const Vob* a, const Vob* b){ return a->position().y < b->position().y; };
      break;
    case 2:
      predicate = [](const Vob* a, const Vob* b){ return a->position().z < b->position().z; };
      break;
    }
  std::sort(v,v+cnt,predicate);
  }

void BaseSpaceIndex::implFind(Vob** v, size_t cnt, uint8_t depth,
                              const Tempest::Vec3& p, float R, const void* ctx, void (*func)(const void*, Vob*)) {
  if(cnt==0)
    return;

  auto mid = cnt/2;
  auto pos = v[mid]->position();
  auto qR  = (R+500.0);//v[mid]->extendedSearchRadius());

  if((pos-p).quadLength()<=qR*qR) {
    func(ctx,v[mid]);
    }

  depth%=3;
  switch(depth) {
    case 0:
      if(p.x-qR<=pos.x)
        implFind(v,mid,uint8_t(depth+1u),p,R, ctx,func);
      if(p.x+qR>=pos.x)
        implFind(v+mid+1,cnt-mid-1,uint8_t(depth+1u),p,R, ctx,func);
      break;
    case 1:
      if(p.y-qR<=pos.y)
        implFind(v,mid,uint8_t(depth+1u),p,R, ctx,func);
      if(p.y+qR>=pos.y)
        implFind(v+mid+1,cnt-mid-1,uint8_t(depth+1u),p,R, ctx,func);
      break;
    case 2:
      if(p.z-qR<=pos.z)
        implFind(v,mid,uint8_t(depth+1u),p,R, ctx,func);
      if(p.z+qR>=pos.z)
        implFind(v+mid+1,cnt-mid-1,uint8_t(depth+1u),p,R, ctx,func);
      break;
    }
  }
