#include "srcparser.h"

#include <Tempest/File>

#include <algorithm>
#include <stdexcept>

using namespace Tempest;

SrcParser::SrcParser(std::u16string_view descr) {
  RFile fin(descr);
  std::string str(fin.size(), '\0');
  if(fin.read(str.data(), str.size()) != str.size())
    throw std::runtime_error("Could not read the entire script manifest");

  auto nextLine = [](std::string_view str, size_t& i) {
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

  size_t at = 0;
  while(at<str.size()) {
    auto line = nextLine(str, at);
    const auto first = line.find_first_not_of(" \t");
    if(first==std::string_view::npos)
      continue;
    line = line.substr(first, line.find_last_not_of(" \t")-first+1);
    if(line.compare(0, 2, "//")==0)
      continue;
    if(line.empty() || line.back()=='/' || line.back()=='\\')
      continue;

    // SRC entries are relative paths and can contain wildcard patterns.
    const auto invalid = [](unsigned char ch) {
      return ch < 0x20 || ch == 0x7f ||
             ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '|';
      };
    if(std::any_of(line.begin(), line.end(), invalid))
      continue;
    files.emplace_back(std::string(line));
    }
  }
