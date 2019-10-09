#include "fileutil.h"

#include <Tempest/Platform>
#ifdef __WINDOWS__
#include <windows.h>
#include <shlwapi.h>
#else
#include <sys/stat.h>
#endif

bool FileUtil::exists(const std::u16string &path) {
#ifdef __WINDOWS__
  return PathFileExistsW(reinterpret_cast<const WCHAR*>(path.c_str()));
#else
  struct stat  buffer={};
  return stat("path to utf8",&buffer);
  return false;
#endif
  }
