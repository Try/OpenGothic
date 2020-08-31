#include "lightgroup.h"

#include "bounds.h"

#include "utils/workers.h"

using namespace Tempest;

LightGroup::LightGroup() {
  }

size_t LightGroup::size() const {
  return light.size();
  }

void LightGroup::set(const std::vector<Light>& l) {
  light = l;
  mkIndex(index,light.data(),light.size(),0);
  dynamicState.clear();
  for(auto& i:light)
    if(i.isDynamic())
      dynamicState.push_back(&i);
  dynamicState.reserve(dynamicState.size());
  }

size_t LightGroup::get(const Bounds& area, const Light** out, size_t maxOut) const {
  return implGet(index,area,out,maxOut);
  }

void LightGroup::tick(uint64_t time) {
  for(auto i:dynamicState)
    i->update(time);
  }

size_t LightGroup::implGet(const LightGroup::Bvh& index, const Bounds& area, const Light** out, size_t maxOut) const {
  const Bvh* cur = &index;
  if(maxOut==0)
    return 0;
  while(true) {
    if(cur->next[0]==nullptr && cur->next[1]==nullptr) {
      size_t cnt = std::min(cur->count,maxOut);
      for(size_t i=0; i<cnt; ++i) {
        out[i] = &cur->b[i];
        }
      return cnt;
      }

    bool l = cur->next[0]!=nullptr && isIntersected(cur->next[0]->bbox,area);
    bool r = cur->next[1]!=nullptr && isIntersected(cur->next[1]->bbox,area);
    if(l && r) {
      size_t cnt0 = implGet(*cur->next[0],area,out,     maxOut);
      size_t cnt1 = implGet(*cur->next[1],area,out+cnt0,maxOut-cnt0);
      return cnt0+cnt1;
      }
    else if(l) {
      cur = cur->next[0].get();
      }
    else if(r) {
      cur = cur->next[1].get();
      }
    else {
      return 0;
      }
    }
  }

void LightGroup::mkIndex(Bvh& id, Light* b, size_t count, int depth) {
  id.b     = b;
  id.count = count;

  if(count==1) {
    id.bbox.assign(b->position(),b->range());
    return;
    }

  depth%=3;
  if(depth==0) {
    std::sort(b,b+count,[](const Light& a,const Light& b){
      return a.position().x<b.position().x;
      });
    }
  else if(depth==1) {
    std::sort(b,b+count,[](const Light& a,const Light& b){
      return a.position().y<b.position().y;
      });
    }
  else {
    std::sort(b,b+count,[](const Light& a,const Light& b){
      return a.position().z<b.position().z;
      });
    }

  size_t half = count/2;
  if(half>0) {
    if(id.next[0]==nullptr)
      id.next[0].reset(new Bvh());
    mkIndex(*id.next[0],b,half,depth+1);
    } else {
    id.next[0].reset();
    }
  if(count-half>0) {
    if(id.next[1]==nullptr)
      id.next[1].reset(new Bvh());
    mkIndex(*id.next[1],b+half,count-half,depth+1);
    } else {
    id.next[1].reset();
    }

  if(id.next[0]!=nullptr && id.next[1]!=nullptr)
    id.bbox.assign(id.next[0]->bbox,id.next[1]->bbox);
  else if(id.next[0]!=nullptr)
    id.bbox = id.next[0]->bbox;
  else if(id.next[1]!=nullptr)
    id.bbox = id.next[1]->bbox;
  }

bool LightGroup::isIntersected(const Bounds& a, const Bounds& b) {
  if(a.bboxTr[1].x<b.bboxTr[0].x)
    return false;
  if(a.bboxTr[0].x>b.bboxTr[1].x)
    return false;
  if(a.bboxTr[1].y<b.bboxTr[0].y)
    return false;
  if(a.bboxTr[0].y>b.bboxTr[1].y)
    return false;
  if(a.bboxTr[1].z<b.bboxTr[0].z)
    return false;
  if(a.bboxTr[0].z>b.bboxTr[1].z)
    return false;
  return true;
  }
