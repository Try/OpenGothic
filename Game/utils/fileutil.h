#pragma once

#include <Tempest/Dir>
#include <string>

namespace FileUtil {
  bool exists(const std::u16string& path);
  std::u16string caseInsensitiveSegment(const std::u16string& path,const char16_t* segment,Tempest::Dir::FileType type);
  std::u16string nestedPath(const std::u16string& gpath, const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type);
  }
