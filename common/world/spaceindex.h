#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <array>
#include <memory>
#include <Tempest/Point>

#include "utils/workers.h"

class Vob;

class BaseSpaceIndex {
  public:
    void   clear();
    size_t size() const { return arr.size(); }
    void   invalidate();

  protected:
    BaseSpaceIndex() = default;
    void               add(Vob* v);
    void               del(Vob* v);
    bool               hasObject(const Vob* v) const;

    void               find(const Tempest::Vec3& p, float R, const void* ctx, void (*func)(const void*, Vob*));
    template<class Func>
    void               parallelFor(Func f);
    Vob**              data() { return arr.data(); }
    Vob*const*         data() const { return arr.data(); }

  private:
    std::vector<Vob*>  arr;
    std::vector<Vob*>  index;
    std::vector<Vob*>  dynamic;

    void               buildIndex();
    void               buildIndex(Vob** v, size_t cnt, uint8_t depth);
    void               sort(Vob** v, size_t cnt, uint8_t component);
    void               implFind(Vob** v, size_t cnt, uint8_t depth, const Tempest::Vec3& p, float R, const void* ctx, void(*func)(const void*, Vob*));
  };

template<class Func>
void BaseSpaceIndex::parallelFor(Func func) {
  Workers::parallelTasks(arr,func);
  }


template<class T>
class SpaceIndex final : public BaseSpaceIndex {
  public:
    SpaceIndex()=default;

    void add(T* v) {
      BaseSpaceIndex::add(v);
      }

    void del(T* v) {
      BaseSpaceIndex::del(v);
      }

    bool hasObject(const T* v) const {
      return BaseSpaceIndex::hasObject(v);
      }

    T**       begin()        { return reinterpret_cast<T**>(data()); }
    T**       end()          { return begin()+size();                }

    T*const*  begin() const  { return reinterpret_cast<T*const*>(data()); }
    T*const*  end()   const  { return begin()+size();                     }

    template<class Func>
    void find(const Tempest::Vec3& p, float R, const Func& f) {
      return BaseSpaceIndex::find(p,R,&f,[](const void* ctx, Vob* v){
        auto& f = *reinterpret_cast<const Func*>(ctx);
        f(*reinterpret_cast<T*>(v));
        });
      }

    template<class F>
    void parallelFor(F func) {
      BaseSpaceIndex::parallelFor([&func](Vob* v){ func(*reinterpret_cast<T*>(v)); });
      }
  };

