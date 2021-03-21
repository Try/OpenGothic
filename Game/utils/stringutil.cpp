#include "stringutil.h"

#include <locale>


std::string StringUtil::toLower(const std::string& str) {
  std::string ret(str);
  
  for(std::string::size_type i = 0; i < ret.length(); i++)
  {
    ret[i] = std::tolower(ret[i], std::locale::classic());
  }
  
  return ret;
  }

std::string StringUtil::toUpper(const std::string& str) {
  std::string ret(str);
  
  for(std::string::size_type i = 0; i < ret.length(); i++)
  {
    ret[i] = std::toupper(ret[i], std::locale::classic());
  }
  
  return ret;
  }

std::string StringUtil::trimFront(const std::string& str, char c) {
  std::string::size_type i;
  
  for(i = 0; i < str.length(); i++) {
    if( str[i] != c ) {
      break;
    }
  }
  
  return str.substr(i);
  }

std::string StringUtil::trimBack(const std::string& str, char c) {
  std::string::size_type i;
  
  for(i = str.length(); i > 0; i--) {
    if( str[i] != c ) {
      break;
    }
  }
  
  return str.substr(0,i);
  }

std::string StringUtil::trim(const std::string& str, char c) {
  std::string ret;
  
  ret = trimFront(str, c);
  ret = trimBack(str, c);
  
  return ret;
  }
  
std::string StringUtil::replace(const std::string& str, char fromC, char toC) {
  std::string ret(str);
  
  for(std::string::size_type i = 0; i < ret.length(); i++)
  {
    if( str[i] == fromC ) {
      ret[i] = toC;
    }
  }
  
  return ret;
}
