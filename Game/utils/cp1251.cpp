#include "cp1251.h"

#ifdef __WINDOWS__
#include <windows.h>
#endif
#include <string>

cp1251::cp1251() {
  tmp.reserve(2048);
  tmpOut.reserve(2048);
  }

cp1251 &cp1251::inst() {
  static cp1251 v;
  return v;
  }

void cp1251::terminate(std::string &s) {
  while(s.size()>0 && s.back()=='\0')
    s.pop_back();
  }

void cp1251::terminate(std::vector<char> &in) {
  while(in.size()>1 && in[in.size()-2]=='\0' && in[in.size()-1]=='\0')
    in.pop_back();
  }

template<class String>
void cp1251::implToUtf8(String &out, const char *in) {
#ifdef __WINDOWS__
  int len = MultiByteToWideChar(1251, 0, in, -1, nullptr, 0);
  if(len<=0){
    out.resize(1);
    out[0]=0;
    return;
    }
  tmp.resize(size_t(len));
  if(!MultiByteToWideChar(1251, 0, in, -1, reinterpret_cast<WCHAR*>(&tmp[0]), len)){
    out.resize(1);
    out[0]=0;
    return;
    }

  len = WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<WCHAR*>(&tmp[0]), -1, nullptr, 0, nullptr, nullptr);
  if(len<=0){
    out.resize(1);
    out[0]=0;
    return;
    }
  out.resize(size_t(len));
  if(!WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<WCHAR*>(&tmp[0]), -1, &out[0], len, nullptr, nullptr)){
    out.resize(1);
    out[0]=0;
    return;
    }
#else
#warning "TODO: implToUtf8"
#endif
  terminate(out);
  }

void cp1251::toUtf8(std::vector<char> &out, const std::string &in) {
  return toUtf8(out,in.c_str());
  }

void cp1251::toUtf8(std::vector<char> &out, const char *in) {
  return inst().implToUtf8(out,in);
  }

const std::string &cp1251::toUtf8(const std::string &in) {
  inst().implToUtf8(inst().tmpOut,in.c_str());
  return inst().tmpOut;
  }

const char *cp1251::toUtf8(const char *in) {
  inst().implToUtf8(inst().tmpOut,in);
  return inst().tmpOut.c_str();
  }
