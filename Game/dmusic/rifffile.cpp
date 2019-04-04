#include "rifffile.h"

#include <algorithm>

using namespace Dx8;

RiffFile::RiffFile(Tempest::IDevice &dev):dev(dev){
  dev.read(&head,sizeof(head));
  }

size_t RiffFile::read(void *to, size_t s) {
  s = std::min(head.size,s);
  s = dev.read(to,s);
  head.size-=s;
  return s;
  }

size_t RiffFile::size() const {
  return head.size; // not valid
  }

size_t RiffFile::seek(size_t advance) {
  size_t c = std::min(advance, head.size);
  c = dev.seek(c);
  head.size-=c;
  return c;
  }

std::u16string RiffFile::readStr() {
  std::u16string s;

  while(true){
    char16_t ch=0;
    read(&ch,2);
    if(ch!=0)
      s.push_back(ch); else
      break;
    }

  return s;
  }


