#include "info.h"

#include <stdexcept>

using namespace  Dx8;

Info::Info(Riff& input) {
  if(!input.is("LIST"))
    throw std::runtime_error("not a LIST");
  input.readListId("INFO");
  input.read([this](Riff& c){
    implRead(c);
    });
  }

void Info::implRead(Riff &input) {
  if(input.is("INAM"))
    input.read(inam);
  }


Unfo::Unfo(Riff& input) {
  if(!input.is("LIST"))
    throw std::runtime_error("not a LIST");
  input.readListId("UNFO");
  input.read([this](Riff& c){
    implRead(c);
    });
  }

void Unfo::implRead(Riff &input) {
  if(input.is("UNAM"))
    input.read(unam);
  }
