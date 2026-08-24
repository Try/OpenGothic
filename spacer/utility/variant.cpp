#include "variant.h"

#include <algorithm>

Variant::Variant() {
  }

Variant::Variant(const Variant& v) {
  tid = v.tid;
  if(tid==nullptr)
    return;
  if(v.staticStorage==v.storage) {
    storage = staticStorage;
    tid->copy(storage,v.storage);
    } else {
    storage = tid->alloc(v.storage);
    }
  }

Variant::Variant(Variant&& v) {
  *this = std::move(v);
  }

Variant::~Variant() {
  freeStorage();
  }

Variant& Variant::operator = (Variant&& v) {
  tid   = v.tid;
  v.tid = nullptr;
  if(tid==nullptr)
    return *this;
  if(v.staticStorage==v.storage) {
    storage = staticStorage;
    tid->copy(storage,v.storage);
    } else {
    std::swap(storage,v.storage);
    }
  return *this;
  }

Variant& Variant::operator = (const Variant& v) {
  Variant tmp(v);
  *this = std::move(tmp);
  return *this;
  }

bool Variant::operator ==(const Variant& other) const {
  if(tid!=other.tid)
    return false;
  if(tid==nullptr)
    return other.tid==nullptr;
  return tid->cmp(storage,other.storage);
  }

bool Variant::operator !=(const Variant& other) const {
  return !(*this==other);
  }

bool Variant::isEmpty() const {
  return tid==nullptr;
  }

void Variant::freeStorage() {
  if(tid==nullptr)
    return;
  tid->destroy(storage);
  if(storage!=staticStorage) {
    std::free(storage);
    storage = nullptr;
    }
  }
