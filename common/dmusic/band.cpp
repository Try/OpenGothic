#include "band.h"

#include <Tempest/Log>
#include <stdexcept>

using namespace Dx8;

Band::Instrument::Instrument(Riff &input) {
  if(!input.isListId("lbin"))
    throw std::runtime_error("not a band instrument");

  input.read([this](Riff& c){
    if(c.is("bins"))
      c.read(&header,sizeof(header));
    else if(c.is("LIST")) {
      if(c.isListId("DMRF"))
        reference = Reference(c);
      }
    });
  }

Band::Band(Riff& input) {
  input.read([this](Riff& c){
    implRead(c);
    });
  }

void Band::implRead(Riff &input) {
  if(input.is("guid"))
    input.read(&guid,sizeof(guid));
  else if(input.is("vers"))
    input.read(&vers,sizeof(vers));
  else if(input.is("LIST")){
    if(input.isListId("lbil")) {
      input.read([this](Riff& c){
        intrument.emplace_back(c);
        });
      }
    else if(input.isListId("UNFO"))
      info = Unfo(input);
    }
  }
