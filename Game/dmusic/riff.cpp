#include "riff.h"

using namespace Dx8;

Riff::Riff(const uint8_t *data, size_t size)
  :data(data), sz(size) {
  readHdr(head);
  sz = std::min(sz,head.size+at);
  }

void Riff::readListId() {
  if(std::memcmp(listId,"\0\0\0\0",4)==0)
    read(&listId,4);
  }

void Riff::readListId(const char *id) {
  readListId();
  if(std::memcmp(listId,id,4)!=0)
    throw std::runtime_error("invalid track");
  }

bool Riff::isListId(const char *id) {
  readListId();
  return (std::memcmp(listId,id,4)!=0)==0;
  }

void Riff::read(std::u16string &str) {
  size_t len = (sz-at)/2;
  if(len<=1){
    str.clear();
    return;
    }
  str.resize(len-1);
  read(&str[0],2*str.size());
  }

void Riff::read(std::string &str) {
  size_t len = (sz-at);
  if(len<=1){
    str.clear();
    return;
    }
  str.resize(len-1);
  read(&str[0],str.size());
  }

void Riff::read(std::vector<uint8_t> &vec) {
  size_t len = (sz-at);
  vec.resize(len);
  if(len>0)
    read(&vec[0],vec.size());
  }

void Riff::read(void *ctx, Riff::Callback cb) {
  while(hasData()) {
    Riff chunk(data+at,sz-at);
    cb(ctx,chunk);
    at+=chunk.head.size+8+(chunk.head.size%2);
    }
  }

void Riff::readHdr(Riff::ChunkHeader &h) {
  read(h.id,   4);
  read(&h.size,4);
  }

void Riff::read(void *dest, size_t s) {
  if(at+s>sz)
    onError("read err");
  std::memcpy(dest,data+at,s);
  at+=s;
  }

void Riff::onError(const char *msg) {
  throw std::runtime_error(msg);
  }
