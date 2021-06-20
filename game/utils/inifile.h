#pragma once

#include <Tempest/File>
#include <vector>

class IniFile final {
  public:
    IniFile(std::u16string file);
    IniFile(Tempest::RFile& fin);

    void flush();

    bool               has (std::string_view sec);
    bool               has (std::string_view sec, std::string_view name);
    int                getI(std::string_view sec, std::string_view name);
    void               set (std::string_view sec, std::string_view name, int ival);
    float              getF(std::string_view sec, std::string_view name);
    void               set (std::string_view sec, std::string_view name, float fval);

    const std::string& getS(std::string_view sec, std::string_view name);
    void               set (std::string_view sec, std::string_view name, std::string_view sval);

  private:
    struct Value final {
      std::string name;
      std::string val;
      };

    struct Section {
      std::string        name;
      std::vector<Value> val;
      };

    void  implRead(Tempest::RFile& fin);
    void  implLine(std::istream &fin);
    char  implSpace(std::istream &fin);
    auto  implName (std::istream &fin) -> std::string;

    void  addSection(std::string&& name);
    void  addValue  (std::string&& name, std::string&& val);
    void  addValue  (Section& sec, std::string&& name, std::string&& val);
    auto  find      (std::string_view sec, std::string_view name) -> Value&;
    auto  find      (std::string_view sec, std::string_view name, bool autoCreate) -> Value*;
    int   getI      (const Value& v) const;
    float getF(const Value& v) const;

    std::vector<Section> sec;
    std::u16string       fileName;
    bool                 changeFlag = false;
  };
