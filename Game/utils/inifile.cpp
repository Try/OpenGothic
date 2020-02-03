#include "inifile.h"

#include <Tempest/Log>
#include <cstring>
#include <sstream>

#include "utils/fileutil.h"

using namespace Tempest;

IniFile::IniFile(std::u16string file) {
  if(!FileUtil::exists(file)) {
    Log::e("no *.ini file in path - using default settnigs");
    return;
    }

  try {
    RFile f(file);
    implRead(f);
    fileName = std::move(file);
    }
  catch (...) {
    Log::e("unable to read .ini file");
    }
  }

IniFile::IniFile(Tempest::RFile &fin) {
  implRead(fin);
  }

void IniFile::flush() {
  if(!changeFlag)
    return;
  changeFlag = false;

  std::stringstream s;
  for(auto& i:sec){
    s << "[" << i.name << "]" << std::endl << std::endl;
    for(auto& r:i.val)
      s << r.name << "=" << r.val << std::endl << std::endl;
    s << std::endl;
    }

  try {
    auto str=s.str();
    WFile f(fileName);
    f.write(&str[0],str.size());
    f.flush();
    }
  catch (...) {
    Log::e("unable to read .ini file");
  }
  }

bool IniFile::has(const char *s, const char *name) {
  return (nullptr != find(s,name,false));
  }

int IniFile::getI(const char *s, const char *name) {
  if(auto* val = find(s,name,false))
    return getI(*val);
  return 0;
  }

void IniFile::set(const char *sec, const char *name, int ival) {
  if(sec==nullptr || std::strlen(sec)==0 || name==nullptr || std::strlen(name)==0)
    return;
  auto& v = find(sec,name);
  v.val = std::to_string(ival);
  changeFlag = true;
  }

const std::string& IniFile::getS(const char* s, const char* name) {
  if(auto* val = find(s,name,false))
    return val->val;
  static std::string empty;
  return empty;
  }

void IniFile::implRead(RFile &fin) {
  size_t sz = fin.size();
  std::string str(sz,'\0');
  fin.read(&str[0],sz);

  std::stringstream s(str);
  while(!s.eof()) {
    implLine(s);
    }
  }

void IniFile::implLine(std::istream &s) {
  char ch=implSpace(s);

  if(ch=='['){
    s.get();
    std::string name = implName(s);
    addSection(std::move(name));
    } else {
    std::string name  = implName(s);
    s.get(ch);
    while(ch!='=' && ch!='\n' && ch!='\r' && ch!='\0' && !s.eof())
      s.get(ch);
    std::string value = implName(s);
    addValue(std::move(name),std::move(value));
    }

  while(ch!='\n' && ch!='\r' && ch!='\0' && !s.eof())
    s.get(ch);
  }

char IniFile::implSpace(std::istream &s) {
  char ch=char(s.peek());
  while(std::isspace(ch) && !s.eof()){
    s.get(ch);
    ch=char(s.peek());
    }
  return ch;
  }

std::string IniFile::implName(std::istream &s) {
  std::string name;
  char ch=char(s.peek());
  while(ch!=']' && ch!='=' && ch!='\n' && ch!='\r' && ch!=';' && !s.eof()){
    name.push_back(ch);
    s.get(ch);
    ch=char(s.peek());
    }
  return name;
  }

void IniFile::addSection(std::string &&name) {
  for(auto& i:sec)
    if(i.name==name)
      return;
  sec.emplace_back();
  sec.back().name = std::move(name);
  }

void IniFile::addValue(std::string &&name, std::string &&val) {
  if(sec.size()==0)
    return;
  addValue(sec.back(),std::move(name),std::move(val));
  }

void IniFile::addValue(Section &sec, std::string &&name, std::string &&val) {
  if(name.size()==0)
    return;
  sec.val.emplace_back();

  auto& v = sec.val.back();
  v.name = std::move(name);
  v.val  = std::move(val);
  }

IniFile::Value& IniFile::find(const char *sec, const char *name) {
  return *find(sec,name,true);
  }

IniFile::Value* IniFile::find(const char *s, const char *name, bool autoCreate) {
  for(auto& i:sec){
    if(i.name==s){
      for(auto& r:i.val)
        if(r.name==name)
          return &r;
      if(autoCreate) {
        addValue(i,name,"");
        return &i.val.back();
        }
      return nullptr;
      }
    }
  if(!autoCreate)
    return nullptr;
  sec.emplace_back();
  sec.back().name = s;
  addValue(sec.back(),name,"");
  return &sec.back().val[0];
  }

int IniFile::getI(const IniFile::Value &v) const {
  try {
    return std::stoi(v.val);
    }
  catch(...) {
    return 0;
    }
  }
