#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

#include <phoenix/world/vob_tree.hh>

class World;
class Serialize;

class Vob {
  public:
    enum Flags:uint8_t {
      None    = 0,
      Startup = 0x1 << 0,
      Static  = 0x1 << 1,
      };
    friend Flags operator | (Flags a, Flags b) { return Flags(uint8_t(a) | uint8_t(b)); }
    friend Flags operator & (Flags a, Flags b) { return Flags(uint8_t(a) & uint8_t(b)); }
    friend Flags operator ~ (Flags a)          { return Flags(~uint8_t(a));             }

    Vob(World& owner);
    Vob(Vob* parent, World& owner, const phoenix::vob& vob, Flags flags);
    virtual ~Vob();
    static std::unique_ptr<Vob> load(Vob* parent, World& world, const phoenix::vob& vob, Flags flags);

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
    virtual float extendedSearchRadius() const;

  protected:
    World&                            world;
    phoenix::vob_type                 vobType = phoenix::vob_type::unknown;
    uint32_t                          vobObjectID = uint32_t(-1);

    virtual void  moveEvent();

  private:
    std::vector<std::unique_ptr<Vob>> child;

    Tempest::Matrix4x4                pos, local;
    Vob*                              parent = nullptr;

    void          recalculateTransform();
  };

