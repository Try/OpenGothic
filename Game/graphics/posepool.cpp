#include "posepool.h"

PosePool::Inst::Inst(const Skeleton& s, const Animation::Sequence* sq, const Animation::Sequence *sq1, uint64_t sTime)
  :pose(s,sq,sq1),sTime(sTime) {
  }

PosePool::PosePool() {
  }

std::shared_ptr<Pose> PosePool::get(const Skeleton *s, const Animation::Sequence *sq1, uint64_t sT, std::shared_ptr<Pose> &prev) {
  return get(s,nullptr,sq1,sT,prev);
  }

std::shared_ptr<Pose> PosePool::get(const Skeleton *s,
                                    const Animation::Sequence *sq, const Animation::Sequence *sq1,
                                    uint64_t sT, std::shared_ptr<Pose> &prev) {
  if(prev.use_count()==1) {
    prev->reset(*s,sq,sq1);
    return prev;
    }

  if(s==nullptr)
    return nullptr;
  if(sq1==nullptr)
    return find(*s);
  // no cache for now, to be parallel friendly
  return find(*s,sq,*sq1,sT);
  }

void PosePool::updateAnimation(uint64_t tickCount) {
  for(auto& i:chunks){
    for(auto& r:i.timed){
      if(r.use_count()>1){
        uint64_t dt=tickCount - r->sTime;
        r->pose.update(dt);
        }
      }
    }
  }

PosePool::Chunk &PosePool::findChunk(const Skeleton& s, const Animation::Sequence* sq0, const Animation::Sequence* sq1) {
  for(auto& i:chunks)
    if(i.skeleton==&s && i.sq0==sq0 && i.sq1==sq1)
      return i;
  chunks.emplace_back(s,sq0,sq1);
  return chunks.back();
  }

std::shared_ptr<Pose> PosePool::find(const Skeleton& s,
                                     const Animation::Sequence* sq0, const Animation::Sequence& sq1, uint64_t sTime) {
  auto i = std::make_shared<Inst>(s,sq0,&sq1,sTime);
  return std::shared_ptr<Pose>(i,&i->pose);
  }

std::shared_ptr<Pose> PosePool::find(const Skeleton &s) {
  auto i = std::make_shared<Inst>(s,nullptr,nullptr,0);
  return std::shared_ptr<Pose>(i,&i->pose);
  }
