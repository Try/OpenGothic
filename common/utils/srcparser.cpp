#include "srcparser.h"

#include <Tempest/Dir>
#include <Tempest/File>
#include <Tempest/TextCodec>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

using namespace Tempest;

static bool matchWildcard(std::string_view file, std::string_view pattern) {
  size_t p = 0, f = 0;
  size_t star  = std::string_view::npos;
  size_t retry = 0;

  while(f < file.size()) {
    if(p < pattern.size() && (pattern[p]=='?' ||
       std::tolower(static_cast<unsigned char>(pattern[p]))==std::tolower(static_cast<unsigned char>(file[f])))) {
      ++p;
      ++f;
      }
    else if(p < pattern.size() && pattern[p]=='*') {
      star = p++;
      retry = f;
      }
    else if(star!=std::string_view::npos) {
      p = star+1;
      f = ++retry;
      }
    else {
      return false;
      }
    }

  while(p < pattern.size() && pattern[p]=='*')
    ++p;

  return p==pattern.size();
  }

static std::string_view nextLine(std::string_view str, size_t& i) {
  auto start = i;
  for(; i<str.size(); ++i) {
    if(str[i]=='\r' && (i+1<str.size() && str[i+1]=='\n')) {
      auto ret = str.substr(start, i-start);
      i+=2;
      return ret;
      }
    if(str[i]=='\n') {
      auto ret = str.substr(start, i-start);
      i++;
      return ret;
      }
    }
  return str.substr(start);
  };


SrcParser::SrcParser(std::u16string_view descr) {
  RFile fin(descr);
  std::string str(fin.size(), '\0');
  if(fin.read(str.data(), str.size()) != str.size())
    throw std::runtime_error("Could not read the entire script manifest");

  auto root = std::filesystem::path(descr).parent_path();
  if(root.empty())
    root = ".";
  const auto directory = root.u16string();

  size_t at = 0;
  while(at<str.size()) {
    auto line = nextLine(str, at);
    const auto first = line.find_first_not_of(" \t");
    if(first==std::string_view::npos)
      continue;

    line = line.substr(first, line.find_last_not_of(" \t")-first+1);
    if(line.compare(0, 2, "//")==0)
      continue;

    // SRC entries are relative paths and can contain wildcard patterns.
    const auto invalid = [](unsigned char ch) {
      return ch < 0x20 || ch == 0x7f ||
             ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '|';
      };
    if(std::any_of(line.begin(), line.end(), invalid))
      continue;

    std::string path(line);
    std::replace(path.begin(), path.end(), '\\', '/');
    if(path.front()=='/' || path.back()=='/' || path.find("//")!=std::string::npos)
      continue;

    const auto split = path.find_last_of('/');
    const auto nameOffset = split==std::string::npos ? 0 : split+1;
    const auto wildcard = path.find_first_of("*?");
    if(wildcard==std::string::npos) {
      files.push_back(std::move(path));
      continue;
      }
    if(wildcard<nameOffset)
      continue;

    const auto prefix = path.substr(0, nameOffset);
    const auto pattern = std::string_view(path).substr(nameOffset);
    const auto searchDir = prefix.empty() ? directory : directory + u"/" + TextCodec::toUtf16(prefix);
    Dir::scan(searchDir, [&](const std::u16string& filename, Dir::FileType type) {
      if(type!=Dir::FT_File)
        return;
      const auto name = TextCodec::toUtf8(filename);
      if(matchWildcard(name, pattern))
        files.push_back(prefix + name);
      });
    }
  }
