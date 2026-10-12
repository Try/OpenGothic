#pragma once

#include <filesystem>
#include <vector>
#include <string_view>

class OpenFileDialog {
  public:
    static std::vector<std::filesystem::path>
              getOpenFileName(std::string_view path,
                              const std::initializer_list<std::pair<std::string_view,std::string_view>>& ext,
                              bool multiselect);
    static std::filesystem::path getProjOpenFileName(std::string_view path);

  private:
    OpenFileDialog() = default;
  };
