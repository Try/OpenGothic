#pragma once

#include <string_view>
#include <vector>
#include <string>

class SrcParser {
  public:
    SrcParser(std::u16string_view descr);

    std::vector<std::string> files;
  };
