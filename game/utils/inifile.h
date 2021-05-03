#pragma once

#include <Tempest/File>
#include <vector>

class IniFile final {
  public:
    IniFile(std::u16string file);
    IniFile(Tempest::RFile& fin);

    void flush();

    bool               has (const char* sec);
    bool               has (const char* sec,const char* name);
    int                getI(const char* sec,const char* name);
    void               set (const char* sec,const char* name,int ival);
    float              getF(const char* sec,const char* name);
    void               set (const char* sec,const char* name,float fval);

    const std::string& getS(const char* sec,const char* name);
    void               set (const char* sec,const char* name,const char* sval);

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
    void  addValue  (Section& sec,std::string&& name, std::string&& val);
    auto  find      (const char* sec,const char* name) -> Value&;
    auto  find      (const char* sec,const char* name,bool autoCreate) -> Value*;
    int   getI      (const Value& v) const;
    float getF(const Value& v) const;

    std::vector<Section> sec;
    std::u16string       fileName;
    bool                 changeFlag = false;
  };
