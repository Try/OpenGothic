#include "pattern.h"

#include <algorithm>
#include <stdexcept>

using namespace Dx8;

Pattern::PartRef::PartRef(Riff &input) {
  if(!input.is("LIST") || !input.isListId("pref"))
    throw std::runtime_error("not a partref");
  input.read([this](Riff& input){
    if(input.is("prfc")) {
      DMUS_IO_PARTREF tmp={};
      const size_t sz = input.remaning();
      input.read(&tmp,std::min(sz,sizeof(tmp)));
      if(sz>sizeof(tmp))
        input.skip(sz-sizeof(tmp));
      io = tmp;
      }
    else if(input.is("LIST") && input.isListId("UNFO"))
     unfo = Unfo(input);
    });
  }

Pattern::Pattern(Riff& input) {
  input.read([this](Riff& c){
    implRead(c);
    });
  }

uint32_t Pattern::timeLength(double tempo) const {
  const uint32_t PulsesPerQuarterNote  = uint32_t(600*(100.0/tempo));
  uint32_t mul = (header.timeSig.bBeatsPerMeasure*PulsesPerQuarterNote*4)/header.timeSig.bBeat;
  return header.wNbrMeasures * mul;
  }

void Pattern::implRead(Riff &input) {
  if(input.is("ptnh"))
    input.read(&header,sizeof(header));
  else if(input.is("LIST")){
    if(input.isListId("pref"))
      partref.emplace_back(input);
    else if(input.isListId("UNFO"))
      info = Unfo(input);
    }
  else if(input.is("rhtm")) {
    const size_t cnt = input.remaning()/sizeof(uint32_t);
    rhythm.resize(cnt);
    for(size_t i=0;i<cnt;++i)
      input.read(&rhythm[i],sizeof(rhythm[i]));
    }
  }

