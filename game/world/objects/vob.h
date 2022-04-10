#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

#include <zenload/zTypes.h>

class World;
class Serialize;

class Vob {
  public:
    Vob(World& owner);
    Vob(Vob* parent, World& owner, ZenLoad::zCVobData& vob, bool startup, bool staticDraw);
    virtual ~Vob();
    static std::unique_ptr<Vob> load(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup, bool staticDraw);

    void          saveVobTree(Serialize& fin) const;
    virtual void  save(Serialize& fout) const;

    void          loadVobTree(Serialize& fin);
    virtual void  load(Serialize& fin);

    Tempest::Vec3 position() const;
    auto          transform() const -> const Tempest::Matrix4x4& { return pos; }
    void          setGlobalTransform(const Tempest::Matrix4x4& p);

    auto          localTransform() const -> const Tempest::Matrix4x4& { return local; }
    void          setLocalTransform(const Tempest::Matrix4x4& p);
    virtual bool  setMobState(std::string_view scheme, int32_t st);

    virtual bool  isDynamic() const;

  protected:
    World&                            world;
    uint8_t                           vobType = 0;
    uint32_t                          vobObjectID = uint32_t(-1);

    virtual void  moveEvent();

  private:
    enum ContentBit : uint8_t {
      cbNone    = 0,
      cbMobsi   = 1 << 0,
      cbTrigger = 1 << 1,
      };
    std::vector<std::unique_ptr<Vob>> child;
    ContentBit                        childContent=cbNone;

    Tempest::Matrix4x4                pos, local;
    Vob*                              parent = nullptr;

    void          recalculateTransform();
  };

