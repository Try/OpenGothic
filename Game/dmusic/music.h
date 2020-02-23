#pragma once

#include <cstdint>
#include <vector>

#include "patternlist.h"

namespace Dx8 {

class PatternList;

class Music final {
  public:
    Music()=default;

    void   addPattern(const PatternList& list);
    void   addPattern(const PatternList& list,size_t id);

    void   clear() { impl->pptn.clear(); }
    size_t size() const { return impl->pptn.size(); }

    void   setVolume(float v);

  private:
    using Pattern = std::shared_ptr<PatternList::PatternInternal>;
    using Groove  = PatternList::Groove;

    struct Internal {
      Internal()=default;
      Internal(const Internal& other);

      std::vector<Pattern> pptn;
      std::vector<Groove>  groove;

      std::atomic<float>   volume{1.f};
      uint64_t             timeTotal=0;
      };
    std::shared_ptr<Internal> impl = std::make_shared<Internal>();

  friend class Mixer;
  };
}
