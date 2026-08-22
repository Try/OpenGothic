#pragma once

#include <cstdint>

class MetaInfo final {
  public:
    template<class T>
    static size_t typeId() {
      static size_t v = nextId();
      return v;
      }
  private:
    static size_t nextId() {
      static size_t v=0;
      v++;
      return v;
      }
  };
