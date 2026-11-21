#pragma once

#include <string_view>
#include <vector>
#include <string>

class SrcParser {
  public:
    SrcParser(std::u16string_view descr);

    // Paths relative to the manifest, with filename wildcards expanded.
    std::vector<std::string> files;
  };
