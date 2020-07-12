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

  protected:
    BaseSpaceIndex() = default;
    void               add(Vob* v);
    void               del(Vob* v);
    bool               hasObject(const Vob* v) const;

    void               find(const Tempest::Vec3& p,float R,void* ctx,void (*func)(void*, Vob*));
    template<class Func>
    void               parallelFor(Func f);
    Vob**              data() { return arr.data(); }
    Vob*const*         data() const { return arr.data(); }

  private:
    std::vector<Vob*>  arr;
    std::vector<Vob*>  index;

    void               buildIndex();
    void               buildIndex(Vob** v, size_t cnt, uint8_t depth);
    void               sort(Vob** v, size_t cnt, uint8_t component);
    void               implFind(Vob** v, size_t cnt, uint8_t depth, const Tempest::Vec3& p, float R, void* ctx, void(*func)(void*, Vob*));
  };

template<class Func>
void BaseSpaceIndex::parallelFor(Func func) {
  Workers::parallelFor(arr,func);
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
    void find(const Tempest::Vec3& p,float R,Func f) {
      return BaseSpaceIndex::find(p,R,&f,[](void* ctx, Vob* v){
        auto& f = *reinterpret_cast<Func*>(ctx);
        f(*reinterpret_cast<T*>(v));
        });
      }

    template<class F>
    void parallelFor(F func) {
      BaseSpaceIndex::parallelFor([&func](Vob* v){ func(*reinterpret_cast<T*>(v)); });
      }
  };


template<class T>
class SpaceIndex2 final : BaseSpaceIndex {
  public:
    SpaceIndex2()=default;

          T& operator[](size_t i)       { return arr[i]; }
    const T& operator[](size_t i) const { return arr[i]; }

    size_t size() const { return arr.size(); }

    T*    begin()        { return arr.data();            }
    T*    end()          { return arr.data()+arr.size(); }

    auto  begin()  const { return arr.data();     }
    auto  end()    const { return arr.data()+arr.size(); }

    auto& back() { return arr.back(); }

    void  pop_back() { arr.pop_back(); index.clear(); }

    void buildIndex() {
      index.resize(arr.size());
      for(size_t i=0;i<index.size();++i){
        index[i] = &arr[i];
        }
      std::sort(index.begin(),index.end(),[](const T* a,const T* b){
        return X(a) < X(b);
        });
      }

    template<class Func>
    void find(const Tempest::Vec3& p,float R,const Func& f) {
      return find(p.x,p.y,p.z,R,f);
      }

    template<class Func>
    void find(float x,float y,float z,float R,Func& f) {
      if(index.size()==0)
        buildIndex();

      auto s = std::lower_bound(index.begin(),index.end(),x-R,[](const T* a,float b){
        return X(a) < b;
        });
      auto e = std::upper_bound(index.begin(),index.end(),x+R,[](float b,const T* a){
        return b < X(a);
        });

      auto RQ = R*R;
      size_t count = std::distance(s,e); (void)count;
      for(auto i=s;i!=e;++i) {
        T* t = *i;
        if( qDist(x,y,z,t)<RQ ){
          if( f(**i) )
            return;
          }
        }
      }

    template<class F>
    void parallelFor(F func) {
      Workers::parallelFor(arr,std::forward(func));
      }

  private:
    std::vector<T>  arr;
    std::vector<T*> index;

    template<class U>
    static float X(const std::unique_ptr<U>* t) {
      return X(t->get());
      }

    template<class U>
    static float qDist(float x,float y,float z,const std::unique_ptr<U>* t) {
      return qDist(x,y,z,t->get());
      }

    template<class U>
    static float X(const U* t) {
      return t->position().x;
      }

    template<class U>
    static float qDist(float x,float y,float z,const U* t) {
      return (Tempest::Vec3(x,y,z)-t->position()).quadLength();
      }
  };
