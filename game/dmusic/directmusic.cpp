#include "directmusic.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

#include <Tempest/File>
#include <utils/fileutil.h>
#include <system_error>

namespace
{
  std::u16string NormalizeReferencePath(std::u16string inPath) {
    std::replace(inPath.begin(), inPath.end(), u'\\', u'/');

    while(!inPath.empty() && (inPath[0] == u'/' || inPath[0] == u'\\'))
      inPath.erase(inPath.begin());

    while(inPath.size() >= 2 && inPath[0] == u'.' && (inPath[1] == u'/' || inPath[1] == u'\\'))
      inPath.erase(inPath.begin(), inPath.begin() + 2);

    return inPath;
    }
}

using namespace Dx8;

DirectMusic::DirectMusic() {
  }

PatternList DirectMusic::load(const Segment &s) {
  return PatternList(s,*this);
  }

PatternList DirectMusic::load(const char16_t *fsgt) {
  Tempest::RFile fin = implOpen(fsgt);
  std::vector<uint8_t> v(size_t(fin.size()));
  fin.read(v.data(), v.size());

  auto r   = Dx8::Riff(v.data(), v.size());
  auto sgt = Dx8::Segment(r);
  return load(sgt);
  }

void DirectMusic::addPath(std::u16string p) {
  path.emplace_back(std::move(p));
  }

const Style &DirectMusic::style(const Reference &id) {
  for(auto& i:styles){
    if(i.first==id.file)
      return i.second;
    }

  Tempest::RFile fin = implOpen(id.file.c_str());
  const size_t length = fin.size();

  std::vector<uint8_t> data(length);
  fin.read(&data[0],data.size());

  Riff  r{data.data(),data.size()};
  Style stl(r);

  styles.emplace_back(id.file,std::move(stl));
  return styles.back().second;
  }

const ChordMap& DirectMusic::chordMap(const Reference& id) {
  return chordMap(id.file);
  }

const ChordMap& DirectMusic::chordMap(const std::u16string& file) {
  for(auto& i:chordMaps){
    if(i.first==file)
      return i.second;
    }

  Tempest::RFile fin = implOpen(file.c_str());
  const size_t length = fin.size();
  if(length==0)
    throw std::runtime_error("invalid chordmap");

  std::vector<uint8_t> data(length);
  fin.read(&data[0],data.size());

  Riff     r{data.data(),data.size()};
  ChordMap map(r);

  chordMaps.emplace_back(file,std::move(map));
  return chordMaps.back().second;
  }

const DlsCollection &DirectMusic::dlsCollection(const Reference &id) {
  return dlsCollection(id.file);
  }

const DlsCollection &DirectMusic::dlsCollection(const std::u16string &file) {
  for(auto& i:dls){
    if(i->first==file)
      return i->second;
    }
  Tempest::RFile fin    = implOpen(file.c_str());
  const size_t   length = fin.size();

  std::vector<uint8_t> data(length);
  fin.read(reinterpret_cast<char*>(&data[0]),data.size());

  Riff          r{data.data(),data.size()};
  DlsCollection stl(r);

  dls.emplace_back(new std::pair<std::u16string,DlsCollection>(file,std::move(stl)));
  return dls.back()->second;
  }

Tempest::RFile DirectMusic::implOpen(const char16_t *file) {
  if(file == nullptr || file[0] == 0)
    throw std::runtime_error("file not found");

  const std::u16string rawPath(file);
  const std::u16string normalizedPath = NormalizeReferencePath(rawPath);

  try {
    std::filesystem::path fsRaw(rawPath);
    if(fsRaw.is_absolute())
      return Tempest::RFile(rawPath);
    }
  catch(std::system_error&) {
    }

  auto tryOpenInSearchPaths = [this](const std::u16string& relPath, const char* errTag) -> Tempest::RFile {
    for(auto& pt:path) {
      try {
        std::u16string filepath = FileUtil::nestedPath(pt, {relPath.c_str()}, Tempest::Dir::FT_File);
        Tempest::RFile fin(filepath);
        return fin;
        }
      catch(std::system_error&) {
        }
      }
    throw std::runtime_error(errTag);
    };

  try {
    return tryOpenInSearchPaths(normalizedPath, "file not found");
    }
  catch(std::runtime_error&) {
    }

  if(normalizedPath != rawPath) {
    try {
      return tryOpenInSearchPaths(rawPath, "file not found");
      }
    catch(std::runtime_error&) {
      }
    }

  for(auto& pt:path) {
    try {
      std::u16string filepath = FileUtil::nestedPath(pt, {file}, Tempest::Dir::FT_File);
      Tempest::RFile fin(filepath);
      return fin;
      }
    catch(std::system_error&){
      }
    }
  throw std::runtime_error("file not found");
  }
