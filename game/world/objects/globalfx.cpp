#include "globalfx.h"

GlobalFx::GlobalFx() {
  }

GlobalFx::GlobalFx(const std::shared_ptr<GlobalEffects::Effect>& h)
  :h(h) {
  }

GlobalFx::GlobalFx(GlobalFx&& other)
  :h(other.h) {
  other.h = nullptr;
  }

GlobalFx& GlobalFx::operator =(GlobalFx&& other) {
  std::swap(h,other.h);
  return *this;
  }

GlobalFx::~GlobalFx() {
  if(h!=nullptr)
    h->timeUntil = 0;
  }

uint64_t GlobalFx::effectPrefferedTime() const {
  return h==nullptr ? 0 : h->timeLen;
  }
