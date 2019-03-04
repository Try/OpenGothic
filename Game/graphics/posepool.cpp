#include "posepool.h"

PosePool::Inst::Inst(const Skeleton& s, const Animation::Sequence* sq, uint64_t sTime)
  :pose(s,sq),sTime(sTime) {
  }

std::shared_ptr<Pose> PosePool::get(const Skeleton *s, const Animation::Sequence *sq, uint64_t sT) {
  if(s==nullptr)
    return nullptr;
  if(sq==nullptr)
    return inst().find(*s);
  return inst().find(*s,*sq,sT);
  }

PosePool::PosePool() {
  }

PosePool& PosePool::inst() {
  static PosePool i;
  return i;
  }

PosePool::Chunk &PosePool::findChunk(const Skeleton& s, const Animation::Sequence* sq) {
  for(auto& i:chunks)
    if(i.skeleton==&s && i.sq==sq)
      return i;
  chunks.emplace_back(s,sq);
  return chunks.back();
  }

std::shared_ptr<Pose> PosePool::find(const Skeleton& s, const Animation::Sequence& sq, uint64_t sTime) {
  sTime = sTime%std::max<uint64_t>(1,uint64_t(sq.totalTime()));

  Chunk& c=findChunk(s,&sq);
  for(auto& i:c.timed)
    if(i->sTime==sTime){
      return std::shared_ptr<Pose>(i,&i->pose);
      }
  auto i = std::make_shared<Inst>(s,&sq,sTime);
  c.timed.push_back(i);
  return std::shared_ptr<Pose>(i,&i->pose);
  }

std::shared_ptr<Pose> PosePool::find(const Skeleton &s) {
  Chunk& c=findChunk(s,nullptr);
  for(auto& i:c.timed)
    if(i->sTime==0){
      return std::shared_ptr<Pose>(i,&i->pose);
      }
  auto i = std::make_shared<Inst>(s,nullptr,0);
  c.timed.push_back(i);
  return std::shared_ptr<Pose>(i,&i->pose);
  }
