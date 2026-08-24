#pragma once

#include <Tempest/File>
#include <Tempest/Vec>

#include <cstdint>
#include <vector>

class JsonWriter final {
  public:
    explicit JsonWriter(Tempest::WFile& out);
    JsonWriter(JsonWriter&& other);
    ~JsonWriter();

    operator bool () const { return out!=nullptr; }

    JsonWriter array (std::string_view name);
    JsonWriter object(std::string_view name);

    void       val(std::string_view name, bool     v);
    void       val(std::string_view name, int32_t  v);
    void       val(std::string_view name, uint32_t v);
    void       val(std::string_view name, uint64_t v);
    void       val(std::string_view name, float    v);
    void       val(std::string_view name, double   v);
    void       val(std::string_view name, std::string_view str);

    void       val(std::string_view name, const Tempest::Vec2& v);
    void       val(std::string_view name, const Tempest::Vec3& v);
    void       val(std::string_view name, const Tempest::Vec4& v);

    void       val(std::string_view name, std::nullptr_t);
    void       val(std::string_view name, const std::vector<float>& v);

    template<class T, std::enable_if_t<!std::is_same<T,uint32_t>::value && !std::is_same<T,uint64_t>::value && std::is_same<T,size_t>::value,bool> = true>
    void       val(std::string_view name, T ) = delete;

  private:
    enum Type : uint8_t {
      None,
      Obj,
      Array
      };

    JsonWriter(Tempest::WFile& out, Type type, size_t depth);

    void implVal    (std::string_view name,const char* v);
    void implWrite  (std::string_view s);
    void implWriteLn(std::string_view s);
    void implNewLine(bool noComma = false);

    Tempest::WFile* out = nullptr;
    Type            type=None;
    size_t          depth=0;
    size_t          counter=0;
  };

