#include "srcparser.h"

#include <Tempest/File>

using namespace Tempest;

SrcParser::SrcParser(std::u16string_view descr) {
  RFile fin(descr);
  std::string str(fin.size(), '\0');
  fin.read(str.data(), str.size());

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
    return str;
    };

  size_t at = 0;
  while(at<str.size()) {
    auto line = nextLine(str, at);
    if(line.empty())
      continue;
    if(line.find("//")==0)
      continue;
    files.emplace_back(std::string(line));
    }
  }
