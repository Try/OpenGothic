#include "music.h"

#include "dlscollection.h"

#include <fstream>
#include "directmusic.h"
#include "style.h"

using namespace Dx8;

Music::Music(const Segment &s, DirectMusic &owner) {
  for(const auto& track : s.track) {
    auto& head = track.head;
    if(head.ckid[0]==0 && std::memcmp(head.fccType,"sttr",4)==0) {
      auto& sttr = *track.sttr;
      for(const auto& style : sttr.styles){
        auto& stl = owner.style(style.reference);
        for(auto& i:stl.band) {
          for(auto& r:i.intrument){
            auto& dls = owner.dlsCollection(r.reference);
            }
          }
        }
      }
    }
  }
