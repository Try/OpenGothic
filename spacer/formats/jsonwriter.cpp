#include "jsonwriter.h"

#include <string>
#include <cstring>
#include <cstdio>

JsonWriter::JsonWriter(Tempest::WFile& out)
  :JsonWriter(out,Obj,0) {
  }

JsonWriter::JsonWriter(JsonWriter&& oth)
  :out(oth.out), type(oth.type), depth(oth.depth), counter(oth.counter) {
  oth.type = None;
  }

JsonWriter::JsonWriter(Tempest::WFile& out, JsonWriter::Type type, size_t depth)
  :out(&out), type(type), depth(0) {
  if(type==Obj)
    implWrite("{");
  if(type==Array)
    implWrite("[");
  this->depth = depth+1;
  }

JsonWriter::~JsonWriter() {
  depth--;
  if(counter>0)
    implNewLine(true);
  char buf[3]={};
  if(type==Obj)
    buf[0] = '}';
  if(type==Array)
    buf[0] = ']';
  implWrite(buf);
  }

JsonWriter JsonWriter::array(std::string_view name) {
  implNewLine();
  char buf[256]={};
  if(type==Obj)
    std::snprintf(buf,sizeof(buf),"\"%.*s\": ",int(name.size()),name.data());
  implWrite(buf);
  counter++;
  return JsonWriter(*out,Array,depth);
  }

JsonWriter JsonWriter::object(std::string_view name) {
  implNewLine();
  char buf[256]={};
  if(type==Obj)
    std::snprintf(buf,sizeof(buf),"\"%.*s\": ",int(name.size()),name.data());
  implWrite(buf);
  counter++;
  return JsonWriter(*out,Obj,depth);
  }

void JsonWriter::val(std::string_view name, bool v) {
  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%s",(v ? "true" : "false"));
  implVal(name,buf);
  }

void JsonWriter::implVal(std::string_view name, const char* v) {
  implNewLine();
  char buf[256];
  if(type==Obj)
    std::snprintf(buf,sizeof(buf),"\"%.*s\":%s",int(name.size()),name.data(),v);
  if(type==Array)
    std::snprintf(buf,sizeof(buf),"%s",v);
  implWrite(buf);
  ++counter;
  }

void JsonWriter::val(std::string_view name, int32_t v) {
  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%d",v);
  implVal(name,buf);
  }

void JsonWriter::val(std::string_view name, uint32_t v) {
  char buf[32]={};
  std::snprintf(buf,sizeof(buf),"%u",v);
  implVal(name,buf);
  }

void JsonWriter::val(std::string_view name, uint64_t v) {
  char buf[32]={};
  std::snprintf(buf,sizeof(buf),"%llu",v);
  implVal(name,buf);
  }

void JsonWriter::val(std::string_view name, float v) {
  char buf[32]={};
  std::snprintf(buf,sizeof(buf),"%.9g",v);
  implVal(name,buf);
  }

void JsonWriter::val(std::string_view name, double v) {
  char buf[32]={};
  std::snprintf(buf,sizeof(buf),"%lf",v);
  implVal(name,buf);
  }

void JsonWriter::val(std::string_view name, const Tempest::Vec2& v) {
  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"[%.9g, %.9g]",v.x,v.y);
  implVal(name,buf);
  }

void JsonWriter::val(std::string_view name, const Tempest::Vec3& v) {
  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"[%.9g, %.9g, %.9g]",v.x,v.y,v.z);
  implVal(name,buf);
  }

void JsonWriter::val(std::string_view name, const Tempest::Vec4& v) {
  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"[%.9g, %.9g, %.9g, %.9g]",v.x,v.y,v.z,v.w);
  implVal(name,buf);
  }

void JsonWriter::val(std::string_view name, std::nullptr_t) {
  implVal(name,"null");
  }

void JsonWriter::val(std::string_view name, const std::vector<float>& v) {
  auto a = array(name);
  for(auto& i:v)
    a.val("",i);
  }

void JsonWriter::val(std::string_view name, std::string_view str) {
  implNewLine();
  if(type==Obj) {
    char buf[256]={};
    std::snprintf(buf,sizeof(buf),"\"%.*s\":",int(name.size()),name.data());
    out->write(buf,std::strlen(buf));
    }

  out->write("\"",1);
  if(str.find("\\")==std::string::npos) {
    out->write(str.data(),str.size());
    } else {
    for(char c:str) {
      if(c=='\\')
        out->write("\\\\",2); else
        out->write(&c,1);
      }
    }
  out->write("\"",1);
  ++counter;
  }

void JsonWriter::implWrite(std::string_view s) {
  out->write(s.data(), s.size());
  }

void JsonWriter::implWriteLn(std::string_view s) {
  implWrite(s);
  out->write("\n",1);
  }

void JsonWriter::implNewLine(bool noComma) {
  if(counter>0 && !noComma) {
    out->write(",",1);
    }
  out->write("\n",1);
  for(size_t i=0;i<depth;++i)
    out->write(" ",1);
  }
