#include "band.h"

#include <stdexcept>

using namespace Dx8;

Band::Instrument::Instrument(Riff &input) {
  char listId[5]={};
  input.read(&listId,4);
  if(std::strcmp(listId,"lbin")!=0)
    throw std::runtime_error("not a band instrument");

  input.read([this](Riff& c){
    if(c.is("bins"))
      c.read(&header,sizeof(header));
    else if(c.is("LIST")){
      char listId[5]={};
      c.read(&listId,4);
      if(std::strcmp(listId,"DMRF")==0)
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
    char listId[5]={};
    input.read(&listId,4);
    if(std::memcmp(listId,"lbil",4)==0){
      input.read([this](Riff& c){
        intrument.emplace_back(c);
        });
      }
    }
  }
