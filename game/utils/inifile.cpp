#include "inifile.h"

#include <Tempest/Log>
#include <cstring>
#include <sstream>
#include <cctype>

#include "utils/fileutil.h"

using namespace Tempest;

static bool compareNoCase(std::string_view a, std::string_view b) {
  if(a.size()!=b.size())
    return false;
  for(size_t i=0; i<a.size(); ++i) {
    char ca = a[i];
    char cb = b[i];
    if('a'<=ca && ca<='z')
      ca = char((ca-'a')+'A');
    if('a'<=cb && cb<='z')
      cb = char((cb-'a')+'A');
    if(ca!=cb)
      return false;
    }
  return true;
  }

IniFile::IniFile(std::u16string_view file) {
  fileName = std::u16string(file);
  if(!FileUtil::exists(fileName)) {
    char name[256] = {};
    size_t li = fileName.find_last_of('/') + 1;
    for(size_t i=li; i<255 && i<file.size(); ++i)
      name[i-li] = char(file.data()[i]);
    Log::e("no \"", name, "\" file in path - using default settings");
    return;
    }

  try {
    RFile f(fileName);
    implRead(f);
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
    Log::e("unable to update .ini file");
    }
  }

bool IniFile::has(std::string_view secName) {
  for(auto& i:sec)
    if(i.name==secName)
      return true;
  return false;
  }

bool IniFile::has(std::string_view s, std::string_view name) {
  return (nullptr != find(s,name,false));
  }

int IniFile::getI(std::string_view s, std::string_view name, int idef) {
  if(auto* val = find(s,name,false))
    return getI(*val);
  return idef;
  }

void IniFile::set(std::string_view sec, std::string_view name, int ival) {
  if(sec.empty() || name.empty())
    return;
  auto& v = find(sec,name);
  v.val = std::to_string(ival);
  changeFlag = true;
  }

float IniFile::getF(std::string_view s, std::string_view name, float fdef) {
  if(auto* val = find(s,name,false))
    return getF(*val);
  return fdef;
  }

void IniFile::set(std::string_view sec, std::string_view name, float fval) {
  if(sec.empty() || name.empty())
    return;
  auto& v = find(sec,name);
  v.val = std::to_string(fval);
  changeFlag = true;
  }

std::string_view IniFile::getS(std::string_view s, std::string_view name) {
  if(auto* val = find(s,name,false))
    return val->val;
  return "";
  }

void IniFile::set(std::string_view sec, std::string_view name, std::string_view sval) {
  if(sec.empty() || name.empty())
    return;
  auto& v = find(sec,name);
  v.val = sval;
  changeFlag = true;
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
    std::string value = implValue(s);
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
  while(ch==' ' && !s.eof()) {
    s.get(ch);
    ch=char(s.peek());
    }
  while(ch!=']' && ch!='=' && ch!='\n' && ch!='\r' && ch!=';' && ch!=' ' && !s.eof()){
    name.push_back(ch);
    s.get(ch);
    ch=char(s.peek());
    }
  return name;
  }

std::string IniFile::implValue(std::istream &s) {
  std::string name;
  char ch=char(s.peek());
  while((ch==' ' || ch=='\t') && !s.eof()) {
    s.get(ch);
    ch=char(s.peek());
    }
  while(ch!='\n' && ch!='\r' && ch!=';' && !s.eof()){
    name.push_back(ch);
    s.get(ch);
    ch=char(s.peek());
    if(ch==' ' || ch=='\t') {
      name.push_back(' ');
      do {
        s.get(ch);
        ch=char(s.peek());
        } while((ch==' ' || ch=='\t') && !s.eof());
      }
    }
  if(!name.empty() && name.back()==' ') {
    name.pop_back();
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

IniFile::Value& IniFile::find(std::string_view sec, std::string_view name) {
  return *find(sec,name,true);
  }

IniFile::Value* IniFile::find(std::string_view s, std::string_view name, bool autoCreate) {
  for(auto& i:sec){
    if(compareNoCase(i.name,s)){
      for(auto& r:i.val)
        if(compareNoCase(r.name,name))
          return &r;
      if(autoCreate) {
        addValue(i,std::string(name),"");
        return &i.val.back();
        }
      return nullptr;
      }
    }
  if(!autoCreate)
    return nullptr;
  sec.emplace_back();
  sec.back().name = s;
  addValue(sec.back(),std::string(name),"");
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

float IniFile::getF(const IniFile::Value& v) const {
  try {
    return float(std::stod(v.val));
    }
  catch(...) {
    return 0;
    }
  }
