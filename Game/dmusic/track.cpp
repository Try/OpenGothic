#include "track.h"

#include <stdexcept>
#include <iostream>

using namespace Dx8;

Track::StyleRef::StyleRef(Riff &input) {
  input.read([&](Riff& ch){
    if(ch.is("stmp"))
      ch.read(&stmp,sizeof(stmp));
    else if(ch.is("LIST")){
      char listId[5]={};
      ch.read(&listId,4);
      if(std::strcmp(listId,"DMRF")==0)
        reference = Reference(ch);
      }
    });
  }

Track::StyleTrack::StyleTrack(Riff &input) {
  input.read([&](Riff& ch){
    if(ch.is("LIST")) {
      char listId[5]={};
      ch.read(&listId,4);
      if(std::strcmp(listId,"strf")==0){
        styles.emplace_back(ch);
        }
      }
    });
  }

Track::Chord::Chord(Riff &input) {
  input.read([&](Riff& ch){
    implReadList(ch);
    });
  }

void Track::Chord::implReadList(Riff &ch) {
  if(ch.is("crdh")) {
    ch.read(&header,4);
    }
  else if(ch.is("crdb")){
    uint32_t ioSzOf=0;
    ch.read(&ioSzOf,4);
    if(ioSzOf>sizeof(ioChord))
      throw std::runtime_error("bad track chord");
    ch.read(&ioChord,ioSzOf);

    uint32_t cSize=0;
    uint32_t cSzOf=0;
    ch.read(&cSize,4);
    ch.read(&cSzOf,4);

    subchord.resize(cSize);
    for(auto& i:subchord){
      ch.read(&i,sizeof(i));
      if(cSzOf>sizeof(i))
        ch.skip(cSzOf-sizeof(i));
      }
    }
  }

Track::Track(Riff &input) {
  if(!input.is("RIFF"))
    throw std::runtime_error("not a riff");

  input.readListId("DMTK");
  input.read([this](Riff& ch) {
    if(ch.is("trkh"))
      ch.read(&head,sizeof(head));
    else if(ch.is("LIST"))
      implReadList(ch);
    });
  }

void Track::implReadList(Riff& input) {
  char listId[5]={};
  input.read(&listId,4);
  if(std::strcmp(listId,"sttr")==0){
    sttr = std::make_shared<StyleTrack>(input);
    }
  else if(std::strcmp(listId,"cord")==0){
    cord = std::make_shared<Chord>(input);
    }
  }
