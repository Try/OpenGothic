#include "installdetect.h"

#include <Tempest/Platform>

#ifdef __WINDOWS__
#include "windows.h"
#include "shlobj.h"
#include "shlwapi.h"
#endif

#ifdef __ANDROID__
#include <android_native_app_glue.h>
extern "C" struct android_app* tempest_android_get_app();
#endif

#include <cstring>
#include "utils/fileutil.h"

InstallDetect::InstallDetect() {
#ifdef __WINDOWS__
  pfiles    = programFiles(false);
  pfilesX86 = programFiles(true);
#endif
#if defined(__OSX__) || defined(__IOS__) || defined(__ANDROID__)
  appDir    = applicationSupportDirectory();
#endif
  }

std::u16string InstallDetect::detectG2() {
#if defined(__WINDOWS__)
  auto ret = detectG2(pfiles);
  if(ret.empty())
    ret = detectG2(pfilesX86);
  return ret;
#elif defined(__OSX__) || defined(__IOS__) || defined(__ANDROID__)
  if(FileUtil::exists(appDir))
    return appDir;
  return u"";
#else
  return u"";
#endif
  }

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

#ifdef __ANDROID__
std::u16string InstallDetect::applicationSupportDirectory() {
  struct android_app* app = tempest_android_get_app();
  if(app == nullptr || app->activity == nullptr)
    return u"";

  // Prefer external data path (accessible without root)
  const char* path = app->activity->externalDataPath;
  if(path == nullptr)
    path = app->activity->internalDataPath;
  if(path == nullptr)
    return u"";

  // Convert UTF-8 to UTF-16
  std::u16string ret;
  size_t len = std::strlen(path);
  ret.reserve(len);
  for(size_t i = 0; i < len; ++i) {
    ret.push_back(static_cast<char16_t>(static_cast<unsigned char>(path[i])));
  }
  return ret;
  }
#endif
