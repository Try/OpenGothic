#include "posepool.h"

PosePool::Inst::Inst(const Skeleton& s, const Animation::Sequence* sq, const Animation::Sequence *sq1, uint64_t sTime)
  :pose(s,sq,sq1),sTime(sTime) {
  }

std::shared_ptr<Pose> PosePool::get(const Skeleton *s, const Animation::Sequence *sq1,uint64_t sT) {
  return get(s,nullptr,sq1,sT);
  }

std::shared_ptr<Pose> PosePool::get(const Skeleton *s, const Animation::Sequence *sq, const Animation::Sequence *sq1, uint64_t sT) {
  if(s==nullptr)
    return nullptr;
  if(sq1==nullptr)
    return inst().find(*s);
  return inst().find(*s,sq,*sq1,sT);
  }

PosePool::PosePool() {
  }

PosePool& PosePool::inst() {
  static PosePool i;
  return i;
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
  sTime = sTime%std::max<uint64_t>(1,uint64_t(sq1.totalTime()));

  Chunk& c=findChunk(s,sq0,&sq1);
  for(auto& i:c.timed)
    if(i->sTime==sTime){
      return std::shared_ptr<Pose>(i,&i->pose);
      }
  auto i = std::make_shared<Inst>(s,sq0,&sq1,sTime);
  c.timed.push_back(i);
  return std::shared_ptr<Pose>(i,&i->pose);
  }

std::shared_ptr<Pose> PosePool::find(const Skeleton &s) {
  Chunk& c=findChunk(s,nullptr,nullptr);
  for(auto& i:c.timed)
    if(i->sTime==0){
      return std::shared_ptr<Pose>(i,&i->pose);
      }
  auto i = std::make_shared<Inst>(s,nullptr,nullptr,0);
  c.timed.push_back(i);
  return std::shared_ptr<Pose>(i,&i->pose);
  }
