#pragma once

#include <vector>
#include <string>

class cp1251 {
  private:
    cp1251();

    static cp1251& inst();

    template<class String>
    void implToUtf8(String& out,const char* in);

    static void terminate(std::string&       in);
    static void terminate(std::vector<char>& in);

    std::vector<char16_t> tmp;
    std::string           tmpOut;

  public:
    static void toUtf8(std::vector<char>& out,const std::string& in);
    static void toUtf8(std::vector<char>& out,const char* in);

    static const std::string& toUtf8(const std::string& in);
    static const char*        toUtf8(const char*        in);
  };
