#pragma once

#include <Tempest/Vec>
#include <vector>

#include "formats/jsonreader.h"
#include "formats/jsonwriter.h"
#include "utility/variant.h"

namespace Property {

enum class CoreType : uint8_t {
  Undefined = 0,
  Template,
  Texture,
  String,
  Vec1,
  Vec2,
  Vec3,
  Vec4,
  };

enum class InputType : uint8_t {
  Float,
  Round,
  Int,
  Bool,
  Percentage,
  Enum
  };

class Type final {
  public:
    CoreType  coreType = CoreType::Texture;
    InputType itype    = InputType::Float;

    Type(CoreType coreType):coreType(coreType){}
    Type(CoreType coreType,InputType it):coreType(coreType),itype(it){}
    virtual ~Type()=default;

    void     saveTypeId(JsonWriter& wr) const;
    static const Type* loadTypeId(const JsonReader& rd);

    static const Type Template;

    static const Type Texture;
    static const Type String;

    static const Type Bool1;
    static const Type Int1;

    static const Type iVec1;
    static const Type iVec2;
    static const Type iVec3;
    static const Type iVec4;

    static const Type Vec1;
    static const Type Vec2;
    static const Type Vec3;
    static const Type Vec4;

    static const Type Enum;

    static const Type Color;
    static const Type Percentage;
  };

struct Slot final {
  Slot()=default;
  Slot(const Type* type, std::string_view name):type(type),name(name) {}
  Slot(const Type* type, std::string_view name, const Tempest::Vec4& min, const Tempest::Vec4& max):type(type),name(name),min(min),max(max) {}
  Slot(const Type* type, std::string_view name, const float   min, const float   max):type(type),name(name),min(min,min,min,min),max(max,max,max,max) {}
  Slot(const Type* type, std::string_view name, const int32_t min, const int32_t max):Slot(type,name,float(min),float(max)) {}
  Slot(const Type* type, std::string_view name, std::initializer_list<const char*> en):type(type),name(name),enumValues(en.size()) {
    for(size_t i=0; i<en.size(); ++i)
      enumValues[i] = *(en.begin()+i);
    }

  const Type*      type   = &Type::Texture;
  std::string_view name   = "";
  uint32_t         uniqId = 0;

  Variant          defValue;
  Tempest::Vec4    min;
  Tempest::Vec4    max = {1,1,1,1};

  std::vector<std::string> enumValues;
  };
}