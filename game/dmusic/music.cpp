#include "music.h"

using namespace Dx8;

Music::Internal::Internal(const Music::Internal& other)
  :pptn(other.pptn),
   groove(other.groove),
   tempo(other.tempo),
   loopStart(other.loopStart),
   loopEnd(other.loopEnd),
   timeTotal(other.timeTotal),
   baseTempo(other.baseTempo) {
  volume = other.volume.load();
  }

void Music::addPattern(const PatternList& list) {
  for(size_t i=0;i<list.size();++i)
    addPattern(list,i);
  }

void Music::addPattern(const PatternList& list, size_t id) {
  auto ptr = std::shared_ptr<PatternList::PatternInternal>(list.intern,&list.intern->pptn[id]);
  if(impl.use_count()>1) {
    impl = std::make_shared<Internal>(*impl);
    }
  impl->pptn.push_back(ptr);
  impl->groove = list.intern->groove;
  impl->tempo  = list.intern->tempo;
  impl->loopStart = list.intern->segmentLoopStart;
  impl->loopEnd   = list.intern->segmentLoopEnd;
  impl->baseTempo = list.intern->baseTempo;
  impl->timeTotal = list.intern->segmentTimeTotal;
  }

void Music::setVolume(float v) {
  impl->volume.store(v);
  }

