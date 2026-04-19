#include "chordmap.h"

#include <algorithm>
#include <stdexcept>

using namespace Dx8;

namespace {

bool equalIgnoreCaseAscii(const std::u16string& a, const std::u16string& b) {
  if(a.size()!=b.size())
    return false;
  for(size_t i=0;i<a.size();++i) {
    char16_t ac = a[i];
    char16_t bc = b[i];
    if(ac>=u'A' && ac<=u'Z')
      ac = char16_t(ac + (u'a' - u'A'));
    if(bc>=u'A' && bc<=u'Z')
      bc = char16_t(bc + (u'a' - u'A'));
    if(ac!=bc)
      return false;
    }
  return true;
  }

}

ChordMap::ChordMap(Riff& input) {
  if(!input.is("RIFF"))
    throw std::runtime_error("not a riff");
  input.readListId("DMPR");
  input.read([this](Riff& ch){
    implRead(ch);
    });
  }

void ChordMap::implRead(Riff& input) {
  if(input.is("perh")) {
    DMUS_IO_CHORDMAP io={};
    const size_t sz = input.remaning();
    input.read(&io,std::min(sz,sizeof(io)));
    if(sz>sizeof(io))
      input.skip(sz-sizeof(io));
    header = io;
    }
  else if(input.is("chdt")) {
    uint16_t itemSize=0;
    if(input.remaning()<sizeof(itemSize))
      return;
    input.read(&itemSize,sizeof(itemSize));
    if(itemSize==0)
      return;

    const size_t count = input.remaning()/itemSize;
    subchords.resize(count);
    for(size_t i=0;i<count;++i) {
      DMUS_IO_CHORDMAP_SUBCHORD sc={};
      input.read(&sc,std::min<size_t>(itemSize,sizeof(sc)));
      if(itemSize>sizeof(sc))
        input.skip(itemSize-sizeof(sc));
      subchords[i] = sc;
      }
    }
  else if(input.is("LIST")) {
    if(input.isListId("chpl")) {
      input.read([this](Riff& ch){
        if(ch.is("LIST") && ch.isListId("chrd")) {
          Chord c;
          readChord(ch,c);
          if(!c.name.empty())
            chordPalette.push_back(std::move(c));
          }
        });
      }
    else if(input.isListId("cmap")) {
      input.read([this](Riff& ch){
        if(!(ch.is("LIST") && ch.isListId("choe")))
          return;

        ChordEntry entry={};
        ch.read([&entry,this](Riff& e){
          if(e.is("cheh")) {
            const size_t sz = e.remaning();
            e.read(&entry.io,std::min(sz,sizeof(entry.io)));
            if(sz>sizeof(entry.io))
              e.skip(sz-sizeof(entry.io));
            }
          else if(e.is("LIST") && e.isListId("chrd"))
            readChord(e,entry.chord);
          });

        if(!entry.chord.name.empty())
          chordEntries.push_back(std::move(entry));
        });
      }
    }
  }

void ChordMap::readChord(Riff& input, Chord& out) {
  input.read([&out](Riff& ch){
    if(ch.is("INAM")) {
      ch.read(out.name);
      while(!out.name.empty() && (out.name.back()==u' ' || out.name.back()==u'\t'))
        out.name.pop_back();
      }
    else if(ch.is("sbcn")) {
      const size_t count = ch.remaning()/sizeof(uint16_t);
      out.subchordIds.resize(count);
      for(size_t i=0;i<count;++i)
        ch.read(&out.subchordIds[i],sizeof(uint16_t));
      }
    });
  }

bool ChordMap::resolve(const Chord& chord, std::vector<DMUS_IO_SUBCHORD>& out) const {
  out.clear();
  for(uint16_t id : chord.subchordIds) {
    if(size_t(id)>=subchords.size())
      continue;
    const auto& src = subchords[size_t(id)];
    DMUS_IO_SUBCHORD dst;
    dst.dwChordPattern    = src.dwChordPattern;
    dst.dwScalePattern    = src.dwScalePattern;
    dst.dwInversionPoints = src.dwInvertPattern;
    dst.dwLevels          = src.dwLevels;
    dst.bChordRoot        = src.bChordRoot;
    dst.bScaleRoot        = src.bScaleRoot;
    out.push_back(dst);
    }
  return !out.empty();
  }

bool ChordMap::resolve(const std::u16string& chordName, std::vector<DMUS_IO_SUBCHORD>& out) const {
  if(chordName.empty())
    return false;

  for(const auto& c : chordEntries)
    if(equalIgnoreCaseAscii(c.chord.name,chordName) && resolve(c.chord,out))
      return true;

  for(const auto& c : chordPalette)
    if(equalIgnoreCaseAscii(c.name,chordName) && resolve(c,out))
      return true;

  return false;
  }
