#include "matrixstorage.h"

#include <cstdint>

#include "graphics/mesh/pose.h"

using namespace Tempest;

MatrixStorage::Id::Id(Id&& other)
  :owner(other.owner), rgn(other.rgn) {
  other.owner = nullptr;
  }

MatrixStorage::Id& MatrixStorage::Id::operator =(Id&& other) {
  std::swap(owner, other.owner);
  std::swap(rgn,   other.rgn);
  return *this;
  }

MatrixStorage::Id::~Id() {
  if(owner!=nullptr)
    owner->free(rgn);
  }

void MatrixStorage::Id::set(const Tempest::Matrix4x4& obj, const Tempest::Matrix4x4* anim) {
  if(owner!=nullptr)
    owner->set(rgn,obj,anim);
  }

void MatrixStorage::Id::set(const Tempest::Matrix4x4& obj, size_t offset) {
  if(owner!=nullptr)
    owner->set(rgn,obj,offset);
  }

const Tempest::StorageBuffer& MatrixStorage::ssbo(uint8_t fId) const {
  return gpu[fId];
  }

bool MatrixStorage::commitUbo(uint8_t fId) {
  auto&  obj = gpu[fId];
  size_t sz  = data.size() * sizeof(Tempest::Matrix4x4);
  if(obj.size()==sz) {
    obj.update(data);
    return false;
    }
  obj = Resources::ssbo(data.data(),sz);
  return true;
  }

MatrixStorage::MatrixStorage() {
  data.reserve(2048);
  data.resize(1);
  data[0].identity();
  }

MatrixStorage::Id MatrixStorage::alloc(size_t nbones) {
  if(nbones==0)
    return Id(*this,Range());

  for(size_t i=0; i<rgn.size(); ++i) {
    if(rgn[i].size==nbones) {
      auto ret = rgn[i];
      rgn.erase(rgn.begin()+intptr_t(i));
      return Id(*this,ret);
      }
    }
  for(size_t i=0; i<rgn.size(); ++i) {
    if(rgn[i].size>nbones) {
      auto ret = rgn[i];
      ret.size = nbones;
      rgn[i].begin += nbones;
      rgn[i].size  -= nbones;
      return Id(*this,ret);
      }
    }
  Range r;
  r.begin = data.size();
  r.size  = nbones;
  data.resize(data.size()+r.size);
  return Id(*this,r);
  }

void MatrixStorage::free(const Range& r) {
  for(auto& i:rgn) {
    if(i.begin+i.size==r.begin) {
      i.size+=r.size;
      return;
      }
    if(r.begin+r.size==i.begin) {
      i.begin -= r.size;
      i.size  += r.size;
      return;
      }
    }
  auto at = std::lower_bound(rgn.begin(),rgn.end(),r,[](const Range& l, const Range& r){
    return l.begin<r.begin;
    });
  rgn.insert(at,r);
  }

void MatrixStorage::set(const Range& rgn, const Matrix4x4& obj, const Tempest::Matrix4x4* mat) {
  //std::memcpy(data.data()+rgn.begin, mat, rgn.size*sizeof(Tempest::Matrix4x4));

  Tempest::Matrix4x4 m[Resources::MAX_NUM_SKELETAL_NODES];
  for(size_t i=0; i<rgn.size; ++i) {
    m[i] = obj;
    m[i].mul(mat[i]);
    }
  std::memcpy(data.data()+rgn.begin, m, rgn.size*sizeof(Tempest::Matrix4x4));

  }

void MatrixStorage::set(const Range& rgn, const Tempest::Matrix4x4& obj, size_t offset) {
  data[rgn.begin+offset] = obj;
  }
