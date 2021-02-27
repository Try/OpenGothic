#pragma once

#include <Tempest/Matrix4x4>
#include <vector>
#include <mutex>
#include <memory>

#include "game/constants.h"
#include "animation.h"

class Skeleton;
class Serialize;
class AnimationSolver;
class Npc;

class Pose final {
  public:
    Pose()=default;

    enum Flags {
      NoFlags       = 0,
      NoTranslation = 1, // usefull for mobsi
      };

    enum StartHint {
      NoHint     = 0x0,
      Force      = 0x1,
      NoInterupt = 0x2,
      };

    static uint8_t     calcAniComb(const Tempest::Vec3& dpos, float rotation);
    static uint8_t     calcAniCombVert(const Tempest::Vec3& dpos);

    void               save(Serialize& fout);
    void               load(Serialize& fin, const AnimationSolver &solver);

    void               setFlags(Flags f);
    BodyState          bodyState() const;
    void               setSkeleton(const Skeleton *sk);
    bool               startAnim(const AnimationSolver &solver, const Animation::Sequence* sq,
                                 uint8_t comb, BodyState bs,
                                 StartHint hint, uint64_t tickCount);
    bool               stopAnim(const char* name);
    bool               stopWalkAnim();
    void               interrupt();
    void               stopAllAnim();
    bool               update(uint64_t tickCount);
    void               processLayers(AnimationSolver &solver, uint64_t tickCount);

    Tempest::Vec3      animMoveSpeed(uint64_t tickCount, uint64_t dt) const;
    void               processSfx(Npc &npc, uint64_t tickCount);
    void               processPfx(MdlVisual& visual, World& world, uint64_t tickCount);
    void               processEvents(uint64_t& barrier, uint64_t now, Animation::EvCount &ev) const;
    bool               isDefParWindow(uint64_t tickCount) const;
    bool               isDefWindow(uint64_t tickCount) const;
    bool               isDefence(uint64_t tickCount) const;
    bool               isJumpBack() const;
    bool               isJumpAnim() const;
    bool               isFlyAnim() const;
    bool               isStanding() const;
    bool               isPrehit(uint64_t now) const;
    bool               isIdle() const;
    bool               isInAnim(const char *sq) const;
    bool               isInAnim(const Animation::Sequence* sq) const;
    bool               hasAnim() const;
    uint64_t           animationTotalTime() const;

    auto               continueCombo(const AnimationSolver &solver,const Animation::Sequence *sq,uint64_t tickCount) -> const Animation::Sequence*;
    uint32_t           comboLength() const;

    float              translateY() const { return trY; }
    auto               bone(size_t id) const -> const Tempest::Matrix4x4&;
    size_t             findNode(const char* b) const;

    void               setRotation(const AnimationSolver &solver, Npc &npc, WeaponState fightMode, int dir);
    bool               setAnimItem(const AnimationSolver &solver, Npc &npc, const char* scheme, int state);
    bool               stopItemStateAnim(const AnimationSolver &solver, uint64_t tickCount);

    const std::vector<Tempest::Matrix4x4>& transform() const;
    const Tempest::Matrix4x4&              transform(size_t id) const;

  private:
    struct Layer final {
      const Animation::Sequence* seq     = nullptr;
      uint64_t                   sAnim   = 0;
      uint8_t                    comb    = 0;
      BodyState                  bs      = BS_NONE;
      };

    auto mkBaseTranslation(const Animation::Sequence *s, BodyState bs) -> Tempest::Matrix4x4;
    void mkSkeleton(const Animation::Sequence &s, BodyState bs);
    void mkSkeleton(const Tempest::Matrix4x4 &mt);
    void mkSkeleton(const Tempest::Matrix4x4 &mt, size_t parent);
    void zeroSkeleton();

    bool updateFrame(const Animation::Sequence &s, uint64_t barrier, uint64_t sTime, uint64_t now);

    const Animation::Sequence* getNext(const AnimationSolver& solver, const Layer& lay);

    void addLayer(const Animation::Sequence* seq, BodyState bs, uint8_t comb, uint64_t tickCount);
    void onAddLayer   (Layer& l);
    void onRemoveLayer(Layer& l);

    template<class T,class F>
    void removeIf(T& t,F f);

    std::vector<Tempest::Matrix4x4> base;

    const Skeleton*                 skeleton=nullptr;
    std::vector<Layer>              lay;
    const Animation::Sequence*      rotation=nullptr;
    int32_t                         itemUseSt     = 0;
    int32_t                         itemUseDestSt = 0;
    float                           trY=0;
    Flags                           flag=NoFlags;
    uint64_t                        lastUpdate=0;
    uint16_t                        comboLen=0;
    bool                            needToUpdate = true;
    uint8_t                         hasEvents = 0;

    std::vector<Tempest::Matrix4x4> tr;
  };
