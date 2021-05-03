#include "mixer.h"
#include "style.h"

#include <stdexcept>
#include <Tempest/Log>

using namespace Tempest;
using namespace Dx8;

Style::Part::Part(Riff &input) {
  input.read([this](Riff& c){
    if(c.is("prth"))
      c.read(&header,sizeof(header));
    else if(c.is("note"))
      c.readAll(notes);
    else if(c.is("crve"))
      c.readAll(curves);
    });
  }

Style::Style(Riff &input) {
  if(!input.is("RIFF"))
    throw std::runtime_error("not a riff");
  input.readListId("DMST");
  input.read([this](Riff& c){
    implRead(c);
    });
  }

void Style::implRead(Riff &input) {
  if(input.is("RIFF")){
    if(input.isListId("DMBD"))
      band.emplace_back(input);
    }
  else if(input.is("guid")){
    input.read(&guid,sizeof(guid));
    }
  else if(input.is("styh")){
    input.read(&styh,sizeof(styh));
    }
  else if(input.is("LIST")){
    if(input.isListId("pttn"))
      patterns.emplace_back(input);
    else if(input.isListId("part"))
      parts.emplace_back(Part(input));
    }
  }

const Style::Part* Style::findPart(const GUID &guid) const {
  for(auto& i:parts)
    if(i.header.guidPartID==guid)
      return &i;
  return nullptr;
  }
