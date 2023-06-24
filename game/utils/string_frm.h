#pragma once

#include <cstdio>
#include <cstring>
#include <string_view>

template<size_t storageSz = 64 /* one cache-line :D */ >
class alignas(64) string_frm {
  public:
    template<class ... Args>
    inline string_frm(const Args&... arg) {
      size_t at = 0;
      implFormat(stk, sizeof(stk)-1, at, arg...);
      if(at+1<sizeof(stk)) {
        stk[at] = '\0';
        return;
        }
      heap                = new char[at+1];
      heap[at]            = '\0';
      stk[sizeof(stk)-1]  = '*';
      at                  = 0;
      implFormat(heap, size_t(-1), at, arg...);
      }

    inline ~string_frm() {
      if(stk[sizeof(stk)-1]!=0)
        delete[] heap;
      }

    string_frm(string_frm&& other) {
      std::swap(stk,other.stk);
      }

    string_frm& operator = (string_frm&& other) {
      std::swap(stk,other.stk);
      return *this;
      }

    inline operator std::string_view() const {
      if(stk[storageSz-1])
        return heap;
      return stk;
      }

    inline bool empty() const {
      if(stk[storageSz-1])
        return heap[0]=='\0';
      return stk[0]=='\0';
      }

    inline char* c_str() {
      if(stk[storageSz-1])
        return heap;
      return stk;
      }

    inline char* begin() {
      if(stk[storageSz-1])
        return heap;
      return stk;
      }

    inline char* end() {
      if(stk[storageSz-1])
        return heap+std::strlen(heap);
      return stk+std::strlen(stk);
      }

    inline const char* begin() const {
      if(stk[storageSz-1])
        return heap;
      return stk;
      }

    inline const char* end() const {
      if(stk[storageSz-1])
        return heap+std::strlen(heap);
      return stk+std::strlen(stk);
      }

    template<size_t sz>
    friend bool operator == (const string_frm<sz>& l, std::string_view v);
    template<size_t sz>
    friend bool operator == (std::string_view v, const string_frm<sz>& l);

  private:
    union {
      char  stk[storageSz] = {};
      char* heap;
      };

    void implFormat(char* out, size_t maxSz, size_t& at) {
      at = 0;
      }

    template<class ... Args>
    void implFormat(char* out, size_t maxSz, size_t& at, const Args&... arg) {
      (implWrite(out,maxSz,at,arg),... );
      }

    // NOTE: const-ref is better for inline-pass in optimizer
    void implWrite(char* out, size_t maxSz, size_t& at, const std::string_view& arg) {
      for(size_t i=0; i<arg.size(); ++i) {
        if(at+i>=maxSz)
          break;
        out[at+i] = arg[i];
        }
      at += arg.size();
      }

    void implWrite(char* out, size_t maxSz, size_t& at, const char* arg) {
      for(size_t i=0; arg[i]; ++i) {
        if(at>=maxSz)
          break;
        out[at] = arg[i];
        at++;
        }
      }

    void implWrite(char* out, size_t maxSz, size_t& at, char arg) {
      if(at<maxSz)
        out[at] = arg;
      ++at;
      }

    void implWrite(char* out, size_t maxSz, size_t& at, int arg) {
      char buf[20] = {};
      std::snprintf(buf,sizeof(buf),"%d",arg);
      implWrite(out, maxSz, at, buf);
      }

    void implWrite(char* out, size_t maxSz, size_t& at, unsigned arg) {
      char buf[20] = {};
      std::snprintf(buf,sizeof(buf),"%u",arg);
      implWrite(out, maxSz, at, buf);
      }

    void implWrite(char* out, size_t maxSz, size_t& at, size_t arg) {
      char buf[20] = {};
      std::snprintf(buf,sizeof(buf),"%llu",uint64_t(arg));
      implWrite(out, maxSz, at, buf);
      }

    void implWrite(char* out, size_t maxSz, size_t& at, float arg) {
      char buf[20] = {};
      std::snprintf(buf,sizeof(buf),"%f",arg);
      implWrite(out, maxSz, at, buf);
      }

    void implWrite(char* out, size_t maxSz, size_t& at, const void* arg) {
      char buf[20] = {};
      std::snprintf(buf,sizeof(buf),"%p",arg);
      implWrite(out, maxSz, at, buf);
      }
  };

template<size_t sz>
bool operator == (const string_frm<sz>& l, std::string_view v) {
  auto ptr = l.begin();
  return v==ptr;
  }

template<size_t sz>
bool operator == (std::string_view v, const string_frm<sz>& l) {
  auto ptr = l.begin();
  return v==ptr;
  }
