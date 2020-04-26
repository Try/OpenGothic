#include "fileutil.h"

#include <Tempest/Platform>
#include <Tempest/TextCodec>

#ifdef __WINDOWS__
#include <windows.h>
#include <shlwapi.h>
#else
#include <sys/stat.h>
#endif

using namespace Tempest;

bool FileUtil::exists(const std::u16string &path) {
#ifdef __WINDOWS__
  return PathFileExistsW(reinterpret_cast<const WCHAR*>(path.c_str()));
#else
  std::string p=Tempest::TextCodec::toUtf8(path);
  struct stat  buffer={};
  return stat(p.c_str(),&buffer)==0;
#endif
  }

std::u16string FileUtil::caseInsensitiveSegment(const std::u16string& path,const char16_t* segment,Dir::FileType type) {
  std::u16string next=path+segment;
  if(FileUtil::exists(next)) {
    if(type==Dir::FT_File)
      return next;
    return next+u"/";
    }

  Dir::scan(path,[&path,&next,&segment,type](const std::u16string& p, Dir::FileType t){
    if(t!=type)
      return;
    for(size_t i=0;;++i) {
      char16_t cs = segment[i];
      char16_t cp = p[i];
      if('A'<=cs && cs<='Z')
        cs = char16_t(cs-'A'+'a');
      if('A'<=cp && cp<='Z')
        cp = char16_t(cp-'A'+'a');

      if(cs!=cp)
        return;
      if(cs=='\0')
        break;
      }

    next = path+p;
    });
  if(type==Dir::FT_File)
    return next;
  return next+u"/";
  }

std::u16string FileUtil::nestedPath(const std::u16string& gpath, const std::initializer_list<const char16_t*> &name, Dir::FileType type) {
    std::u16string path = gpath;
    for(auto& segment:name)
    path = caseInsensitiveSegment(path,segment, (segment==*(name.end()-1)) ? type : Dir::FT_Dir);
  return path;
  }
