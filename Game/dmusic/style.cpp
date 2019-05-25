#include "mixer.h"
#include "style.h"

#include <stdexcept>
#include <Tempest/Log>

using namespace Tempest;
using namespace Dx8;

// From the Microsoft DX8 SDK docs
static uint32_t musicOffset(uint32_t mtGridStart, int16_t nTimeOffset, const DMUS_IO_TIMESIG& timeSig) {
  const uint32_t ppq  = 768;
  const uint32_t mult = (ppq*4)/timeSig.bBeat;
  return uint32_t(nTimeOffset) +
         (mtGridStart / timeSig.wGridsPerBeat) * (mult) +
         (mtGridStart % timeSig.wGridsPerBeat) * (mult/timeSig.wGridsPerBeat);
  }

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
    char listId[5]={};
    input.read(&listId,4);
    if(std::memcmp(listId,"DMBD",4)==0)
      band.emplace_back(input);
    }
  else if(input.is("guid")){
    input.read(&guid,sizeof(guid));
    }
  else if(input.is("LIST")){
    char listId[5]={};
    input.read(&listId,4);
    if(std::memcmp(listId,"pttn",4)==0)
      patterns.emplace_back(input);
    else if(std::memcmp(listId,"part",4)==0)
      parts.emplace_back(Part(input));
    }
  }

const Style::Part* Style::findPart(const GUID &guid) const {
  for(auto& i:parts)
    if(i.header.guidPartID==guid)
      return &i;
  return nullptr;
  }

void Style::exec(const Pattern &p) const {
  for(size_t i=0;i<p.partref.size();++i) {
    auto& px = p.partref[i];
    if(auto part = findPart(px.io.guidPartID)){
      if(part->notes.size()>0 || part->curves.size()>0) {
        std::string st(px.unfo.unam.begin(),px.unfo.unam.end());
        Log::i("part: ",i," ",st," partid=",px.io.wLogicalPartID);
        exec(*part);
        }
      }
    }

  mix(p);
  }

void Style::exec(const Style::Part &part) const {
  for(auto& i:part.notes){
    uint32_t time = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig);

    Log::i("  note: ",time," - ",time+i.mtDuration," var=",i.dwVariation);
    }
  }

void Style::mix(const Pattern &p) const {
  uint32_t patternLength = p.timeLength();

  Mixer mx(patternLength);

  for(size_t i=0;i<p.partref.size();++i) {
    auto& px = p.partref[i];
    if(auto part = findPart(px.io.guidPartID)){
      if(part->notes.size()>0 || part->curves.size()>0) {
        mix(mx,*part);
        }
      }
    }
  }

void Style::mix(Mixer &m, const Style::Part &part) const {
  for(auto& i:part.notes){
    uint32_t time = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig);

    //Log::i("  note: ",time," - ",time+i.mtDuration);
    }
  }
