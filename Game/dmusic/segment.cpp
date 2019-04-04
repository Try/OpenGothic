#include "segment.h"

#include <Tempest/Log>
#include <stdexcept>

#include "rifffile.h"

using namespace Dx8;
using namespace Tempest;

Segment::ReferenceList::ReferenceList(RiffFile &dev) {
  while(dev.hasData()) {
    RiffFile::readChunk(dev,*this,[](RiffFile& f,ReferenceList& rl){
      if(f.is("refh")){
        f.read(&rl.header,sizeof(rl.header));
        }
      else if(f.is("guid")){
        f.read(&rl.guid,sizeof(rl.guid));
        }
      else if(f.is("name")){
        rl.name = f.readStr();
        }
      else if(f.is("file")){
        rl.file = f.readStr();
        }
      else if(f.is("catg")){
        rl.category = f.readStr();
        }
      else if(f.is("vers")){
        f.read(&rl.version,sizeof(rl.version));
        }
      });
    }
  }

Segment::TrackCoord::TrackCoord(RiffFile &dev) {
  while(dev.hasData()) {
    RiffFile::readChunk(dev,*this,[](RiffFile& f,TrackCoord& t){
      if(f.is("crdh")){
        f.read(&t.header,sizeof(t.header));
        }
      else if(f.is("crdb")){
        std::uint32_t chordSz=0;
        f.read(&chordSz,sizeof(chordSz));

        }
      });
    }
  }

Segment::TrackStyle::TrackStyle(RiffFile &dev) {
  while(dev.hasData()) {
    RiffFile::readChunk(dev,*this,[](RiffFile& dev,TrackStyle& t){
      if(dev.is("LIST")) {
        char listId[5]={};
        dev.read(&listId,4);
        if(std::memcmp(listId,"strf",4)==0)
          t.readStyle(dev);
        }
      });
    }
  }

void Segment::TrackStyle::readStyle(RiffFile &dev) {
  std::uint16_t stmp=0, *pstmp=nullptr;

  while(dev.hasData()) {
    RiffFile::readChunk(dev,*this,[&](RiffFile& f,TrackStyle& t){
      if(f.is("stmp")){
        f.read(&stmp,sizeof(stmp));
        }
      else if(f.is("LIST") && pstmp==nullptr){
        char listId[5]={};
        f.read(&listId,4);
        if(std::memcmp(listId,"DMRF",4)==0){
          t.styles.emplace_back(std::make_pair(0,Segment::ReferenceList(f)));
          pstmp = &t.styles.back().first;
          }
        }
      });
    }

  if(pstmp)
    *pstmp = stmp;
  }


Segment::Segment(Tempest::IDevice &dev) {
  RiffFile::readChunk(dev,root,[this](RiffFile& f,Chunk& ch){
    readChunk(f,ch);
    });
  }

void Segment::readChunk(RiffFile &dev, Chunk &ch) {
  std::memcpy(&ch.id,dev.id(),sizeof(ch.id));

  if(dev.is("RIFF")) {
    dev.read(&ch.listId,sizeof(ch.listId));
    readNested(dev,ch);
    }
  else if(dev.is("LIST")) {
    dev.read(&ch.listId,sizeof(ch.listId));
    if(ch.listIs("UNFO")){
      // author info; skip it
      }
    else if(ch.listIs("trkl")){
      readTracks(dev,ch);
      }
    }
  else if(dev.is("seqt")) {
    readNested(dev,ch);
    }
  else if(dev.is("guid")) {
    dev.read(&guid,sizeof(guid));
    }
  else if(dev.is("segh")) {
    dev.read(&header,sizeof(header));
    }
  else if(dev.is("vers")) {
    dev.read(&version,sizeof(version));
    }
  }

void Segment::readNested(RiffFile &dev, Segment::Chunk &ch) {
  while(dev.hasData()) {
    ch.nested.emplace_back();
    auto& b = ch.nested.back();
    RiffFile::readChunk(dev,b,[this](RiffFile& f,Chunk& ch){
      readChunk(f,ch);
      });
    }
  }

void Segment::readTracks(RiffFile &dev, Segment::Chunk&) {
  while(dev.hasData()) {
    RiffFile::readChunk(dev,*this,[](RiffFile& f,Segment& s){
      if(f.is("RIFF")){
        s.tracks.emplace_back();
        f.seek(4); // list-id
        s.readTrack(f,s.tracks.back());
        }
      });
    }
  }

void Segment::readTrack(RiffFile& dev, Track& tr) {
  while(dev.hasData()) {
    RiffFile::readChunk(dev,tr,[](RiffFile& f,Track& tr){
      if(f.is("trkh")){
        f.read(&tr.header,sizeof(tr.header));
        }
      else if(f.is("vers")){
        f.read(&tr.version,sizeof(tr.version));
        }
      else if(f.is("guid")){
        f.read(&tr.guid,sizeof(tr.guid));
        }
      else if(f.is("LIST")){
        char listId[5]={};
        f.read(&listId,4);
        if(std::memcmp(listId,"cord",4)==0){
          tr.data.reset(new TrackCoord(f));
          }
        else if(std::memcmp(listId,"sttr",4)==0){
          tr.data.reset(new TrackStyle(f));
          }
        else {
          Log::d("unknown tag: \"",listId,"\"");
          }
        }
      });
    }
  }


