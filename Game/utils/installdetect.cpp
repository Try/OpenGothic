#include "installdetect.h"

#include <Tempest/Platform>

#ifdef __WINDOWS__
#include "windows.h"
#include "shlobj.h"
#include "shlwapi.h"
#endif

#include <cstring>

InstallDetect::InstallDetect() {
#ifdef __WINDOWS__
  pfiles = programFiles();
#endif
  }

std::u16string InstallDetect::detectG2() {
  auto akela = pfiles+u"/Akella Games/Gothic II/";
  if(check(akela))
    return akela;
  auto steam = pfiles+u"/Steam/steamapps/common/Gothic II/";
  if(check(steam))
    return steam;
  return u"";
  }

bool InstallDetect::check(const std::u16string& path) {
#ifdef __WINDOWS__
  return PathFileExistsW(reinterpret_cast<const WCHAR*>(path.c_str()));
#else
  return false; //TODO
#endif
  }

#ifdef __WINDOWS__
std::u16string InstallDetect::programFiles() {
  WCHAR path[MAX_PATH]={};
  if(FAILED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, path)))
     return u"";
  std::u16string ret;
  size_t len=0;
  for(;path[len];++len);

  ret.resize(len);
  std::memcpy(&ret[0],path,len*sizeof(char16_t));
  return ret;
  }
#endif
