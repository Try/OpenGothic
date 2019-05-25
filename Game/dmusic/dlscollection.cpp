#include "dlscollection.h"

#include <stdexcept>

using namespace Dx8;

DlsCollection::Region::Region(Riff &input) {
  if(!input.is("LIST"))
    throw std::runtime_error("not a riff");
  input.readListId("rgn ");

  }

DlsCollection::Instrument::Instrument(Riff &input) {
  if(!input.is("LIST"))
    throw std::runtime_error("not a list");
  input.readListId("ins ");
  input.read([this](Riff& c){
    implRead(c);
    });
  }

void DlsCollection::Instrument::implRead(Riff &input) {
  if(input.is("LIST")){
    if(input.isListId("lrgn")){
      input.read([this](Riff& c){
        regions.emplace_back(c);
        });
      }
    }
  }

DlsCollection::DlsCollection(Riff& input) {
  if(!input.is("RIFF"))
    throw std::runtime_error("not a riff");
  input.readListId("DLS ");
  input.read([this](Riff& c){
    implRead(c);
    });
  }

void DlsCollection::implRead(Riff &input) {
  if(input.is("vers"))
    input.read(&version,sizeof(version));
  else if(input.is("dlid"))
    input.read(&dlid,sizeof(dlid));
  else if(input.is("LIST")){
    if(input.isListId("wvpl")){
      input.read([this](Riff& chunk){
        wave.emplace_back(chunk);
        });
      }
    else if(input.isListId("lins")){
      input.read([this](Riff& chunk){
        instrument.emplace_back(chunk);
        });
      }
    }
  }
