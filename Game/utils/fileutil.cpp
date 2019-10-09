#include "fileutil.h"

#include <Tempest/Platform>
#include <Tempest/TextCodec>

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
  std::string p=Tempest::TextCodec::toUtf8(path);
  struct stat  buffer={};
  return stat(p.c_str(),&buffer)==0;
#endif
  }
