#include "track.h"

#include <stdexcept>
#include <iostream>
#include <algorithm>

using namespace Dx8;

Track::StyleRef::StyleRef(Riff &input) {
  input.read([&](Riff& ch){
    if(ch.is("stmp")) {
      uint32_t t=0;
      const size_t sz = ch.remaning();
      ch.read(&t,std::min(sz,sizeof(t)));
      if(sz>sizeof(t))
        ch.skip(sz-sizeof(t));
      stmp = t;
      }
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

Track::ChordMapRef::ChordMapRef(Riff &input) {
  input.read([&](Riff& ch){
    if(ch.is("stmp")) {
      uint32_t t=0;
      const size_t sz = ch.remaning();
      ch.read(&t,std::min(sz,sizeof(t)));
      if(sz>sizeof(t))
        ch.skip(sz-sizeof(t));
      stmp = t;
      }
    else if(ch.is("LIST")){
      char listId[5]={};
      ch.read(&listId,4);
      if(std::strcmp(listId,"DMRF")==0)
        reference = Reference(ch);
      }
    });
  }

Track::ChordMapTrack::ChordMapTrack(Riff &input) {
  input.read([&](Riff& ch){
    if(ch.is("LIST")) {
      char listId[5]={};
      ch.read(&listId,4);
      if(std::strcmp(listId,"pfrf")==0)
        chordMaps.emplace_back(ch);
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
    Event evt;
    evt.header = header;

    uint32_t ioSzOf=0;
    ch.read(&ioSzOf,4);
    if(ioSzOf>sizeof(evt.ioChord))
      throw std::runtime_error("bad track chord");
    ch.read(&evt.ioChord,ioSzOf);

    uint32_t cSize=0;
    uint32_t cSzOf=0;
    ch.read(&cSize,4);
    ch.read(&cSzOf,4);

    evt.subchord.resize(cSize);
    for(auto& i:evt.subchord){
      ch.read(&i,sizeof(i));
      if(cSzOf>sizeof(i))
        ch.skip(cSzOf-sizeof(i));
      }

    ioChord  = evt.ioChord;
    subchord = evt.subchord;
    events.push_back(std::move(evt));
    }
  }

Track::CommandTrack::CommandTrack(Riff& input) {
  uint32_t sz32=0;
  input.read(&sz32,4);

  size_t sz = sz32;

  size_t count = input.remaning()/sz;
  commands.resize(count);

  for(uint32_t i=0;i<count;++i) {
    DMUS_IO_COMMAND& io=commands[i];
    input.read(&io,std::min(sz,sizeof(io)));
    if(sz>sizeof(io))
      input.skip(sz-sizeof(io));
    }
  }

Track::TempoTrack::TempoTrack(Riff& input) {
  uint32_t sz32=0;
  input.read(&sz32,4);
  const size_t sz = sz32;
  if(sz==0)
    return;

  const size_t count = input.remaning()/sz;
  events.reserve(count);

  for(size_t i=0;i<count;++i) {
    Event e;
    const size_t itemBegin = input.remaning();
    if(sz>=4)
      input.read(&e.time,4);
    if(sz>=8) {
      uint32_t pad=0;
      input.read(&pad,4);
      }
    if(sz>=16)
      input.read(&e.tempo,8);

    const size_t consumed = itemBegin - input.remaning();
    if(sz>consumed)
      input.skip(sz-consumed);

    events.push_back(e);
    }
  }


Track::Track(Riff &input) {
  if(!input.is("RIFF"))
    throw std::runtime_error("not a riff");

  input.readListId("DMTK");
  input.read([this](Riff& ch) {
    if(ch.is("trkh"))
      ch.read(&head,sizeof(head));
    else if(ch.is("cmnd"))
      cmnd = std::make_shared<CommandTrack>(ch);
    else if(ch.is("tetr"))
      tetr = std::make_shared<TempoTrack>(ch);
    else if(ch.is("LIST"))
      implReadList(ch);
    });
  }

void Track::implReadList(Riff& input) {
  if(input.isListId("sttr")){
    sttr = std::make_shared<StyleTrack>(input);
    }
  else if(input.isListId("pftr")){
    pftr = std::make_shared<ChordMapTrack>(input);
    }
  else if(input.isListId("cord")){
    cord = std::make_shared<Chord>(input);
    }
  }
