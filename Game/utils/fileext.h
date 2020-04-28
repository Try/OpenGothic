#pragma once

#include <string>
#include <cstring>
#include <cctype>

namespace FileExt {
  inline bool hasExt(const std::string& s,const char* extIn) {
    const size_t l = std::strlen(extIn);
    if(l+1>s.size())
      return false;
    const size_t off = s.size()-l;
    if(s[off-1]!='.')
      return false;
    for(size_t i=0;i<l;++i) {
      char a = char(std::tolower(s[off+i]));
      char b = char(std::tolower(extIn[i]));
      if(a!=b)
        return false;
      }
    return true;
    }

  inline bool exchangeExt(std::string& s,const char* extIn,const char* extOut) {
    if(!hasExt(s,extIn))
      return false;
    const size_t l1 = std::strlen(extIn);
    const size_t l2 = std::strlen(extOut);
    if(l1<l2)
      s.resize(s.size()+l2-l1); else
    if(l1>l2)
      s.resize(s.size()+l1-l2);

    const size_t off = s.size()-l2;
    for(size_t i=0;i<l2;++i)
      s[off+i] = extOut[i];
    return true;
    }
  }
