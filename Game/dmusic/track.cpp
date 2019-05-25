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
  }

