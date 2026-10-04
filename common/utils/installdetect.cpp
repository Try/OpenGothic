#include "installdetect.h"

#include <Tempest/Platform>

#ifdef __ANDROID__
#include <Tempest/AndroidApi>
#endif

#ifdef __WINDOWS__
#include "windows.h"
#include "shlobj.h"
#include "shlwapi.h"
#endif

#include <cstring>
#include "utils/fileutil.h"

InstallDetect::InstallDetect() {
#ifdef __WINDOWS__
  pfiles    = programFiles(false);
  pfilesX86 = programFiles(true);
#endif
#if defined(__OSX__) || defined(__IOS__)
  appDir    = applicationSupportDirectory();
#endif
  }

std::u16string InstallDetect::detectG2() {
#if defined(__WINDOWS__)
  auto ret = detectG2(pfiles);
  if(ret.empty())
    ret = detectG2(pfilesX86);
  return ret;
#elif defined(__OSX__) || defined(__IOS__)
  if(FileUtil::exists(appDir))
    return appDir;
  return u"";
#elif defined(__ANDROID__)
  auto path = Tempest::AndroidApi::externalDataPath();
  if(path.empty())
    return u"";
  path /= "Gothic2";
  std::filesystem::create_directories(path);
  return path.u16string();
#else
  return u"";
#endif
  }

#ifdef __ANDROID__
std::filesystem::path InstallDetect::androidInternalDataPath() {
  auto path = Tempest::AndroidApi::internalDataPath();
  std::filesystem::create_directories(path);
  return path;
  }
#endif

std::u16string InstallDetect::detectG2(std::u16string pfiles) {
  if(pfiles.empty())
    return u"";
  auto steam = pfiles+u"/Steam/steamapps/common/Gothic II/";
  if(FileUtil::exists(steam))
    return steam;
  auto akela = pfiles+u"/Akella Games/Gothic II/";
  if(FileUtil::exists(akela))
    return akela;
  return u"";
  }

#ifdef __WINDOWS__
std::u16string InstallDetect::programFiles(bool x86) {
  WCHAR path[MAX_PATH]={};
  if(FAILED(SHGetFolderPathW(NULL, (x86 ? CSIDL_PROGRAM_FILESX86 : CSIDL_PROGRAM_FILES), NULL, 0, path)))
    return u"";
  std::u16string ret;
  size_t len=0;
  for(;path[len];++len);

  ret.resize(len);
  std::memcpy(&ret[0],path,len*sizeof(char16_t));
  return ret;
  }
#endif
