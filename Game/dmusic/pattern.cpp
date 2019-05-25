#include "pattern.h"

#include <stdexcept>

using namespace Dx8;

Pattern::PartRef::PartRef(Riff &input) {
  if(!input.is("LIST") || !input.isListId("pref"))
    throw std::runtime_error("not a partref");
  input.read([this](Riff& input){
    if(input.is("prfc"))
      input.read(&io,sizeof(io));
    else if(input.is("LIST") && input.isListId("UNFO"))
     unfo = Unfo(input);
    });
  }

Pattern::Pattern(Riff& input) {
  input.read([this](Riff& c){
    implRead(c);
    });
  }

uint32_t Pattern::timeLength() const {
  static const std::uint32_t PulsesPerQuarterNote = 768;
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
  }

