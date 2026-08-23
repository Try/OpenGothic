#include "jsonreader.h"

#include <Tempest/Except>
#include <Tempest/Log>

#include <rapidjson/error/en.h>

using namespace rapidjson;
using namespace Tempest;

JsonReader::JsonReader(Tempest::RFile& fin) : json(fin.size(),' '), ownDoc(true) {
  if(fin.read(json.data(),json.size())!=json.size())
    throw std::system_error(Tempest::SystemErrc::UnableToOpenFile);
  initDocument();
  }

JsonReader::JsonReader(const std::string& str)
  :json(str), ownDoc(true) {
  initDocument();
  }

JsonReader::JsonReader(const char* str)
  :json(str), ownDoc(true) {
  initDocument();
  }

void JsonReader::initDocument() {
  std::unique_ptr<Document> d(new Document());
  d->ParseInsitu<kParseTrailingCommasFlag>(json.data());
  if(d->HasParseError()){
    const char* err = GetParseError_En(d->GetParseError());
    Log::d("node-json parse error: \"",err,"\"");
    return;
    }
  doc = d.release();
  val = doc;
  }

JsonReader::JsonReader(const JsonReader* owner) : doc(owner->doc) {
  }

JsonReader::JsonReader(JsonReader&& other)
  :doc(other.doc), val(other.val), ownDoc(other.ownDoc){
  other.ownDoc = false;
  }

JsonReader::~JsonReader() {
  if(ownDoc)
    delete doc;
  }

bool JsonReader::isArray() const {
  return val!=nullptr && val->IsArray();
  }

bool JsonReader::isObject() const {
  return val!=nullptr && val->IsObject();
  }

size_t JsonReader::size() const {
  return isArray() ? val->Size() : 0;
  }

JsonReader JsonReader::operator [](const char* name) const {
  JsonReader rd(this);
  if(val!=nullptr && val->IsObject() && val->HasMember(name))
    rd.val = &(*val)[name];
  return rd;
  }

JsonReader JsonReader::operator [](std::string_view name) const {
  char buf[128] = {};
  std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
  JsonReader rd(this);
  if(val!=nullptr && val->IsObject() && val->HasMember(buf))
    rd.val = &(*val)[buf];
  return rd;
  }

JsonReader JsonReader::operator [](size_t id) const {
  JsonReader rd(this);
  if(isArray())
    rd.val = &(*val)[SizeType(id)];
  return rd;
  }

bool JsonReader::isString() const {
  return val!=nullptr && val->IsString();
  }

std::string_view JsonReader::toString() const {
  if(isString())
    return val->GetString();
  return "";
  }

std::filesystem::path JsonReader::toPath() const {
  if(isString())
    return std::filesystem::path(reinterpret_cast<const char8_t*>(val->GetString()));
  return "";
  }

bool JsonReader::isUint() const {
  return val!=nullptr && val->IsUint();
  }

uint32_t JsonReader::toUint() const {
  if(isUint())
    return val->GetUint();
  return 0;
  }

bool JsonReader::isInt() const {
  return val!=nullptr && val->IsInt();
  }

int32_t JsonReader::toInt() const {
  if(isInt())
    return val->GetInt();
  return 0;
  }

bool JsonReader::isBool() const {
  return val!=nullptr && val->IsBool();
  }

bool JsonReader::toBool() const {
  if(isBool())
    return val->GetBool();
  return false;
  }

bool JsonReader::isFloat() const {
  return val!=nullptr && val->IsNumber();
  }

float JsonReader::toFloat() const {
  if(isFloat())
    return val->GetFloat();
  return 0;
  }

void JsonReader::toArray(std::vector<float>& out) const {
  if(!isArray()) {
    out.clear();
    return;
    }
  out.resize(size());
  for(size_t i=0; i<out.size(); ++i) {
    auto& el = (*val)[SizeType(i)];
    if(el.IsNumber())
      out[i] = el.GetFloat(); else
      out[i] = 0;
    }
  }

Vec4 JsonReader::toVec(uint8_t destLen) const {
  if(val==nullptr)
    return Vec4();
  if(isFloat())
    return Vec4(val->GetFloat(),0,0,0);
  if(isArray()) {
    float vx[4] = {};
    for(size_t i=0;i<destLen && i<size();++i) {
      auto& el = (*val)[SizeType(i)];
      if(el.IsNumber())
        vx[i] = el.GetFloat();
      }
    return Vec4(vx[0],vx[1],vx[2],vx[3]);
    }
  return Vec4();
  }

Vec2 JsonReader::toVec2() const {
  auto v = toVec(2);
  return Vec2(v.x,v.y);
  }

Vec3 JsonReader::toVec3() const {
  auto v = toVec(3);
  return Vec3(v.x,v.y,v.z);
  }

Vec4 JsonReader::toVec4() const {
  auto v = toVec(4);
  return Vec4(v.x,v.y,v.z,v.w);
  }

