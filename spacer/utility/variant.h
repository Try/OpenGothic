#pragma once

#include <cstdlib>
#include <utility>
#include <typeinfo>
#include <new>

class Variant final {
  public:
    Variant();
    template<class T>
    explicit Variant(const T& v);
    explicit Variant(const Variant& v);
    Variant(Variant&& v);
    ~Variant();

    Variant& operator = (Variant&& v);
    Variant& operator = (const Variant& v);

    bool operator == (const Variant& other) const;
    bool operator != (const Variant& other) const;

    bool isEmpty() const;

    template<class T>
    void     set(const T& t);

    template<class T>
    bool     get(T& out) const;

    template<class T>
    T*       get();
    template<class T>
    const T* get() const;

  private:
    struct Helper {
#ifndef NDEBUG
      const char* tag = nullptr;
#endif
      void*  (*alloc)  (const void*)              = nullptr;
      void   (*destroy)(void*)                    = nullptr;
      void   (*copy)   (void*,const void*)        = nullptr;
      void   (*move)   (void*,void*)              = nullptr;
      bool   (*cmp)    (const void*, const void*) = nullptr;
      };

    template<class T>
    static Helper& helper();

    void    freeStorage();

    Helper* tid              = nullptr;
    void*   storage          = nullptr;
    char    staticStorage[8] = {};
  };

template<class T>
Variant::Helper& Variant::helper() {
  struct _ {
    static void* alloc(const void* data) {
      auto& t = *reinterpret_cast<const T*>(data);
      void* s = std::malloc(sizeof(t));
      if(s==nullptr)
        throw std::bad_alloc();
      new(s) T(t);
      return s;
      }

    static void destroy(void* data) {
      T& t = *reinterpret_cast<T*>(data);
      t.~T();
      }
    static void copy(void* dest,const void* src) {
      auto& d = *reinterpret_cast<T*>(dest);
      auto& s = *reinterpret_cast<const T*>(src);

      d = s;
      }
    static void move(void* dest,void* src) {
      auto& d = *reinterpret_cast<T*>(dest);
      auto& s = *reinterpret_cast<T*>(src);

      d = std::move(s);
      }
    static bool compare(const void* dest,const void* src) {
      auto& d = *reinterpret_cast<const T*>(dest);
      auto& s = *reinterpret_cast<const T*>(src);

      return (d == s);
      }
    };

  static Helper h;
#ifndef NDEBUG
  h.tag     = typeid(T).name();
#endif
  h.alloc   = &_::alloc;
  h.destroy = &_::destroy;
  h.copy    = &_::copy;
  h.move    = &_::move;
  h.cmp     = &_::compare;

  return h;
  }


template<class T>
Variant::Variant(const T& t) {
  set(t);
  }

template<class T>
void Variant::set(const T& t) {
  auto& h = helper<T>();
  if(&h==tid) {
    h.copy(storage,&t);
    } else {
    freeStorage();
    tid = nullptr;
    if(sizeof(staticStorage)<sizeof(t)) {
      storage = h.alloc(&t);
      } else {
      storage = staticStorage;
      h.copy(storage,&t);
      }
    tid = &h;
    }
  }

template<class T>
bool Variant::get(T& out) const {
  auto& h = helper<T>();
  if(&h==tid) {
    out = *reinterpret_cast<T*>(storage);
    return true;
    }
  return false;
  }

template<class T>
T* Variant::get() {
  auto& h = helper<T>();
  if(&h==tid)
    return reinterpret_cast<T*>(storage);
  return nullptr;
  }

template<class T>
const T* Variant::get() const {
  auto& h = helper<T>();
  if(&h==tid)
    return reinterpret_cast<T*>(storage);
  return nullptr;
  }
