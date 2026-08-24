#include "segment.h"

#include <stdexcept>
#include <iostream>

using namespace Dx8;

Segment::Segment(Riff &input) {
  if(!input.is("RIFF"))
    throw std::runtime_error("not a riff");

  char listId[5]={};
  input.read(&listId,4);
  input.read([this](Riff& ch){
    if(ch.is("LIST")){
      implReadList(ch);
      }
    });
  }

void Segment::implReadList(Riff& input) {
  if(input.isListId("trkl")){
    input.read([&](Riff& ch){
      track.emplace_back(ch);
      });
    }
  }
