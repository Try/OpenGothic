#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

class PfxBucket;

class PfxEmitter final {
  public:
    PfxEmitter()=default;
    ~PfxEmitter();
    PfxEmitter(PfxEmitter&&);
    PfxEmitter& operator=(PfxEmitter&& b);

    PfxEmitter(const PfxEmitter&)=delete;

    bool     isEmpty() const { return bucket==nullptr; }
    void     setPosition (float x,float y,float z);
    void     setPosition (const Tempest::Vec3& pos);
    void     setTarget   (const Tempest::Vec3& pos);
    void     setDirection(const Tempest::Matrix4x4& pos);
    void     setObjMatrix(const Tempest::Matrix4x4& mt);
    void     setActive(bool act);
    bool     isActive() const;
    void     setLooped(bool loop);

    uint64_t effectPrefferedTime() const;

  private:
    PfxEmitter(PfxBucket &b,size_t id);

    PfxBucket* bucket = nullptr;
    size_t     id     = size_t(-1);

  friend class PfxBucket;
  friend class PfxObjects;
  };

