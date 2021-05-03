#include "dlscollection.h"

#include <Tempest/Log>
#include <Tempest/MemWriter>
#include <stdexcept>
#include <fstream>

#include "soundfont.h"

using namespace Dx8;
using namespace Tempest;

DlsCollection::Region::Region(Riff &input) {
  if(!input.is("LIST"))
    throw std::runtime_error("not a riff");
  input.readListId("rgn ");
  input.read([this](Riff& c){
    implRead(c);
    });
  }

void DlsCollection::Region::implRead(Riff &input) {
  if(input.is("rgnh")){
    input.read(&head,sizeof(head));
    }
  else if(input.is("LIST") && input.isListId("lart")){
    // articulators
    }
  else if(input.is("wlnk")){
    input.read(&wlink,sizeof(wlink));
    }
  else if(input.is("wsmp")){
    input.read(&waveSample,sizeof(waveSample));
    loop.resize(waveSample.cSampleLoops);
    for(auto& i:loop){
      input.read(&i,sizeof(i));
      input.skip(i.cbSize-sizeof(i));
      }
    }
  }


DlsCollection::Articulator::Articulator(Riff &input) {
  if(!input.is("art1"))
    throw std::runtime_error("not a riff");

  uint32_t cbSize=0;
  uint32_t size=0;

  input.read(&cbSize,sizeof(cbSize));
  input.read(&size,sizeof(size));
  connectionBlocks.resize(size);

  for(auto& i:connectionBlocks) {
    input.read(&i,sizeof(i));
    }
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
    else if(input.isListId("lart")){
      input.read([this](Riff& c){
        articulators.emplace_back(c);
        });
      }
    else if(input.isListId("INFO")){
      info = Info(input);
      }
    }
  else if(input.is("insh")){
    input.read(&header,sizeof(header));
    }
  }

DlsCollection::DlsCollection(Riff& input) {
  if(!input.is("RIFF"))
    throw std::runtime_error("not a riff");
  input.readListId("DLS ");
  input.read([this](Riff& c){
    implRead(c);
    });

  shData = SoundFont::shared(*this,wave);
  wave.clear();
  }

void DlsCollection::implRead(Riff &input) {
  if(input.is("vers"))
    input.read(&version,sizeof(version));
  else if(input.is("dlid"))
    input.read(&dlid,sizeof(dlid));
  else if(input.is("LIST")){
    if(input.isListId("wvpl")){
      input.read([this](Riff& chunk){
        Wave wx(chunk);
        wave.emplace_back(std::move(wx));
        });
      }
    else if(input.isListId("lins")){
      input.read([this](Riff& chunk){
        instrument.emplace_back(chunk);
        });
      }
    }
  }

SoundFont DlsCollection::toSoundfont(uint32_t dwPatch) const {
  return SoundFont(shData,dwPatch);
  }

void DlsCollection::dbgDump() const {
  Log::i("__DLS__");

  for(auto& i:instrument){
    for(auto& r:i.regions) {
      if(r.wlink.ulTableIndex<wave.size()) {
        Log::i(" ",r.head.RangeKey.usLow,"-",r.head.RangeKey.usHigh," ",r.wlink.ulTableIndex);//," ",wave[r.wlink.ulTableIndex].info.inam);
        } else {
        Log::i(" ",r.wlink.ulTableIndex);
        }
      }
    }
  }

const Wave* DlsCollection::findWave(uint8_t note) const {
  for(auto& i:instrument){
    for(auto& r:i.regions) {
      if(r.head.RangeKey.usLow<=note && note<=r.head.RangeKey.usHigh) {
        if(r.wlink.ulTableIndex<wave.size())
          return &wave[r.wlink.ulTableIndex];
        }
      }
    }
  return nullptr;
  }
