#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <array>

template<class T>
class SpaceIndex {
  public:
    SpaceIndex()=default;

    template<typename... Args>
    void emplace_back(Args&&... args) {
      index.clear();
      arr.emplace_back(std::forward<Args>(args)...);
      }

          T& operator[](size_t i){ return arr[i]; }
    const T& operator[](size_t i) const { return arr[i]; }

    size_t size() const { return arr.size(); }

    auto begin()        { return arr.begin(); }
    auto end()          { return arr.end();   }

    auto begin()  const { return arr.begin(); }
    auto end()    const { return arr.end();   }

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
    void find(const std::array<float,3>& p,float R,const Func& f) {
      return find(p[0],p[1],p[2],R,f);
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
        if( qDist(x,y,z,*i)<RQ ){
          if( f(**i) )
            return;
          }
        }
      }

  private:
    std::vector<T>  arr;
    std::vector<T*> index;

    static float X(const T* t) {
      return t->position()[0];
      }

    static float qDist(float x,float y,float z,const T* t) {
      x -= t->position()[0];
      y -= t->position()[1];
      z -= t->position()[2];

      return x*x + y*y + z*z;
      }
  };
