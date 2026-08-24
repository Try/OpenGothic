#pragma once

#include <Tempest/File>
#include <Tempest/Vec>

#include <filesystem>
#include <string_view>
#include <vector>

class ProjectItem;

class JsonReader final {
  public:
    explicit JsonReader(Tempest::RFile& fin);
    explicit JsonReader(const std::string& str);
    explicit JsonReader(const char* str);
    JsonReader(JsonReader&& other);
    ~JsonReader();

    operator bool () const { return val!=nullptr; }

    bool          isArray()  const;
    bool          isObject() const;
    size_t        size() const;

    JsonReader    operator [](const char* name) const;
    JsonReader    operator [](std::string_view name) const;
    JsonReader    operator [](size_t id) const;

    bool          isString() const;
    std::string_view toString() const;
    auto          toPath()   const -> std::filesystem::path;

    bool          isUint() const;
    uint32_t      toUint() const;

    bool          isInt() const;
    int32_t       toInt() const;

    bool          isBool() const;
    bool          toBool() const;

    bool          isFloat() const;
    float         toFloat() const;

    void          toArray(std::vector<float>& out) const;

    Tempest::Vec4 toVec(uint8_t destLen) const;
    Tempest::Vec2 toVec2() const;
    Tempest::Vec3 toVec3() const;
    Tempest::Vec4 toVec4() const;

  private:
    JsonReader(const JsonReader* owner);
    void initDocument();

    class Document;
    class Value;

    std::string json;
    Document*   doc    = nullptr;
    Value*      val    = nullptr;
    bool        ownDoc = false;
  };

