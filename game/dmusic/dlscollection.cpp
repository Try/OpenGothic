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
  if(input.isListId("rgn "))
    input.readListId("rgn ");
  else if(input.isListId("rgn2"))
    input.readListId("rgn2");
  else
    throw std::runtime_error("not a region");
  input.read([this](Riff& c){
    implRead(c);
    });
  }

void DlsCollection::Region::implRead(Riff &input) {
  if(input.is("rgnh")){
    input.read(&head,sizeof(head));
    }
  else if(input.is("LIST") && (input.isListId("lart") || input.isListId("lar2"))){
    input.read([this](Riff& c){
      Articulator art(c);
      articulationConnections.insert(articulationConnections.end(),
                                     art.connectionBlocks.begin(),
                                     art.connectionBlocks.end());
      });
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
  const bool isArt1 = input.is("art1");
  const bool isArt2 = input.is("art2");
  if(!isArt1 && !isArt2)
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
    else if(input.isListId("lart") || input.isListId("lar2")){
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

DlsCollection::DlsCollection(Riff& input, const bool inKeepWaveData) {
  keepWaveData = inKeepWaveData;
  if(!input.is("RIFF"))
    throw std::runtime_error("not a riff");
  input.readListId("DLS ");
  input.read([this](Riff& c){
    implRead(c);
    });

  waveNameByIndex.reserve(wave.size());
  waveSampleRateByIndex.reserve(wave.size());
  for(const auto& w : wave) {
    waveNameByIndex.emplace_back(w.info.inam);
    waveSampleRateByIndex.emplace_back(w.wfmt.dwSamplesPerSec);
    }

  shData = SoundFont::shared(*this,wave);
  if(!keepWaveData)
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
  bool preferDrumKit = (dwPatch & 0x80000000u)!=0u;
  if(const Instrument* ins = findInstrument(dwPatch))
    preferDrumKit = preferDrumKit || ((ins->header.Locale.ulBank & 0x80000000u)!=0u);
  return SoundFont(shData, dwPatch, preferDrumKit);
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

const Wave* DlsCollection::waveByTableIndex(const uint32_t tableIndex) const {
  if(tableIndex>=wave.size())
    return nullptr;
  return &wave[tableIndex];
  }

const DlsCollection::Instrument* DlsCollection::findInstrument(uint32_t dwPatch) const {
  const uint8_t bankHi = uint8_t((dwPatch & 0x00FF0000) >> 0x10);
  const uint8_t bankLo = uint8_t((dwPatch & 0x0000FF00) >> 0x8);
  const uint8_t patch  = uint8_t(dwPatch & 0x000000FF);
  const uint32_t legacyBank = (uint32_t(bankHi) << 16) + uint32_t(bankLo);

  // Prefer later instruments when duplicates exist, mirroring DirectMusic behavior.
  for(size_t idx=instrument.size(); idx>0; --idx) {
    const auto& i = instrument[idx-1];
    const uint8_t instrPatch = uint8_t(i.header.Locale.ulInstrument & 0xFFu);
    if(instrPatch!=patch)
      continue;

    const uint32_t instrBankRaw = i.header.Locale.ulBank;
    const uint8_t instrBankHi = uint8_t((instrBankRaw >> 16) & 0xFFu);
    const uint8_t instrBankLo = uint8_t(instrBankRaw & 0xFFu);
    if(instrBankRaw==legacyBank || (instrBankHi==bankHi && instrBankLo==bankLo))
      return &i;
    }

  // Fallback: many legacy files effectively use only one bank byte.
  for(size_t idx=instrument.size(); idx>0; --idx) {
    const auto& i = instrument[idx-1];
    if(uint8_t(i.header.Locale.ulInstrument & 0xFFu)!=patch)
      continue;
    if(uint8_t(i.header.Locale.ulBank & 0x7Fu) == uint8_t(bankLo & 0x7Fu))
      return &i;
    }

  // Last resort: by program only.
  for(size_t idx=instrument.size(); idx>0; --idx) {
    const auto& i = instrument[idx-1];
    if(uint8_t(i.header.Locale.ulInstrument & 0xFFu) == patch)
      return &i;
    }
  return nullptr;
  }

const std::string& DlsCollection::waveNameByTableIndex(uint32_t tableIndex) const {
  static const std::string kEmpty;
  if(tableIndex>=waveNameByIndex.size())
    return kEmpty;
  return waveNameByIndex[tableIndex];
  }

uint32_t DlsCollection::waveSampleRateByTableIndex(uint32_t tableIndex) const {
  if(tableIndex>=waveSampleRateByIndex.size())
    return 0;
  return waveSampleRateByIndex[tableIndex];
  }
