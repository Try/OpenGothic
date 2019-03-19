#include "posepool.h"

PosePool::Inst::Inst(const Skeleton& s, const Animation::Sequence* sq, const Animation::Sequence *sq1, uint64_t sTime)
  :pose(s,sq,sq1),sTime(sTime) {
  }

PosePool::PosePool() {
  }

std::shared_ptr<Pose> PosePool::get(const Skeleton *s, const Animation::Sequence *sq1,uint64_t sT) {
  return get(s,nullptr,sq1,sT);
  }

std::shared_ptr<Pose> PosePool::get(const Skeleton *s, const Animation::Sequence *sq, const Animation::Sequence *sq1, uint64_t sT) {
  if(s==nullptr)
    return nullptr;
  if(sq1==nullptr)
    return find(*s);
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
  uint64_t fr  = std::max<uint64_t>(1000u/std::max<uint64_t>(uint64_t(sq1.fpsRate),1),1);
  sTime        = sTime%std::max<uint64_t>(1,uint64_t(sq1.totalTime()));
  size_t frame = size_t(sTime)/size_t(fr);

  Chunk& c=findChunk(s,sq0,&sq1);
  if(sq0==nullptr){
    if(c.timed.size()==0)
      c.timed.resize(sq1.numFrames+1);
    auto lk = c.timed[frame];
    if(lk!=nullptr)
      return std::shared_ptr<Pose>(lk,&lk->pose);
    auto i = std::make_shared<Inst>(s,sq0,&sq1,sTime);
    c.timed[frame] = i;
    return std::shared_ptr<Pose>(i,&i->pose);
    }

  for(auto& i:c.timed) {
    if(i && i->sTime==sTime){
      return std::shared_ptr<Pose>(i,&i->pose);
      }
    }
  auto i = std::make_shared<Inst>(s,sq0,&sq1,sTime);
  c.timed.push_back(i);
  return std::shared_ptr<Pose>(i,&i->pose);
  }

std::shared_ptr<Pose> PosePool::find(const Skeleton &s) {
  Chunk& c=findChunk(s,nullptr,nullptr);
  for(auto& i:c.timed){
    //auto i = pi.lock();
    if(i->sTime==0){
      return std::shared_ptr<Pose>(i,&i->pose);
      }
    }
  auto i = std::make_shared<Inst>(s,nullptr,nullptr,0);
  c.timed.push_back(i);
  return std::shared_ptr<Pose>(i,&i->pose);
  }
