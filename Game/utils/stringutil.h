#pragma once

#include <string>

namespace StringUtil {
  std::string toLower(const std::string& str);
  std::string toUpper(const std::string& str);
  std::string trimFront(const std::string& str, char c);
  std::string trimBack(const std::string& str, char c);
  std::string trim(const std::string& str, char c);
  std::string replace(const std::string& str, char fromC, char toC);
  }
