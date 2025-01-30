#pragma once

#include <Tempest/Matrix4x4>
#include <vector>
#include <mutex>
#include <memory>

#include "game/constants.h"
#include "animation.h"
#include "resources.h"

class Skeleton;
class Serialize;
class AnimationSolver;
class Npc;

class Pose final {
  public:
    Pose();

    enum Flags {
      NoFlags       = 0,
      NoTranslation = 1, // useful for mobsi
      };

    enum StartHint : uint8_t {
      NoHint     = 0x0,
      Force      = 0x1,
      };

    static uint8_t     calcAniComb(const Tempest::Vec3& dpos, float rotation);
    static uint8_t     calcAniCombVert(const Tempest::Vec3& dpos);

    void               save(Serialize& fout);
    void               load(Serialize& fin, const AnimationSolver &solver);

    void               setFlags(Flags f);
    BodyState          bodyState() const;
    bool               hasState(BodyState s) const;
    bool               hasStateFlag(BodyState f) const;
    void               setSkeleton(const Skeleton *sk);
    bool               startAnim(const AnimationSolver &solver, const Animation::Sequence* sq,
                                 uint8_t comb, BodyState bs,
                                 StartHint hint, uint64_t tickCount);
    bool               stopAnim(std::string_view name);
    bool               stopWalkAnim();
    void               interrupt();
    void               stopAllAnim();

    void               setObjectMatrix(const Tempest::Matrix4x4& obj, bool sync);
    bool               update(uint64_t tickCount);

    void               processLayers(AnimationSolver &solver, uint64_t tickCount);
    bool               processEvents(uint64_t& barrier, uint64_t now, Animation::EvCount &ev) const;

    Tempest::Vec3      animMoveSpeed(uint64_t tickCount, uint64_t dt) const;
    void               processSfx(Npc &npc, uint64_t tickCount);
    void               processPfx(MdlVisual& visual, World& world, uint64_t tickCount);
    bool               isDefParWindow(uint64_t tickCount) const;
    bool               isDefWindow(uint64_t tickCount) const;
    bool               isDefence(uint64_t tickCount) const;
    bool               isJumpBack() const;
    bool               isJumpAnim() const;
    bool               isFlyAnim() const;
    bool               isStanding() const;
    bool               isPrehit(uint64_t now) const;
    bool               isAttackAnim() const;
    bool               isIdle() const;
    bool               isInAnim(std::string_view           sq) const;
    bool               isInAnim(const Animation::Sequence* sq) const;
    bool               hasAnim() const;
    uint64_t           animationTotalTime() const;
    uint64_t           atkTotalTime() const;

    auto               continueCombo(const AnimationSolver &solver, const Animation::Sequence *sq, BodyState bs, uint64_t tickCount) -> const Animation::Sequence*;
    uint16_t           comboLength() const;

    float              translateY() const { return trY; }
    auto               rootNode() const -> const Tempest::Matrix4x4;
    auto               rootBone() const -> const Tempest::Matrix4x4;
    auto               bone(size_t id) const -> const Tempest::Matrix4x4&;
    size_t             boneCount() const;
    size_t             findNode(std::string_view b) const;

    void               setHeadRotation(float dx, float dz);
    Tempest::Vec2      headRotation() const;
    void               setAnimRotate(const AnimationSolver &solver, Npc &npc, WeaponState fightMode, enum TurnAnim turn, int dir);
    auto               setAnimItem(const AnimationSolver &solver, Npc &npc, std::string_view scheme, int state) -> const Animation::Sequence*;
    bool               stopItemStateAnim(const AnimationSolver &solver, uint64_t tickCount);

    const Tempest::Matrix4x4* transform() const;

  private:
    enum SampleStatus : uint8_t {
      S_None  = 0,
      S_Old   = 1,
      S_Valid = 2,
      };

    struct Layer final {
      const Animation::Sequence* seq      = nullptr;
      uint64_t                   sAnim    = 0;
      uint64_t                   sBlend   = 0;
      uint8_t                    comb     = 0;
      BodyState                  bs       = BS_NONE;
      };

    struct ComboState {
      uint16_t bits = 0;
      uint16_t len()     const { return bits & 0x7FFF; }
      void     incLen()        { bits = (bits+1)&0x7FFF; };
      bool     isBreak() const { return bits & 0x8000; }
      void     setBreak()      { bits |=0x8000; }
      };

    auto mkBaseTranslation() -> Tempest::Vec3;
    void mkSkeleton(const Tempest::Matrix4x4 &mt);
    void implMkSkeleton(const Tempest::Matrix4x4 &mt);
    void implMkSkeleton(const Tempest::Matrix4x4 &mt, size_t parent);

    bool updateFrame(const Animation::Sequence &s, BodyState bs, uint64_t sBlend, uint64_t barrier, uint64_t sTime, uint64_t now);

    const Animation::Sequence* solveNext(const AnimationSolver& solver, const Layer& lay);

    void addLayer(const Animation::Sequence* seq, BodyState bs, uint8_t comb, uint64_t tickCount);
    void onAddLayer   (const Layer& l);
    void onRemoveLayer(const Layer& l);

    static bool hasLayerEvents(const Layer& l);

    template<class T,class F>
    void removeIf(T& t,F f);

    const Skeleton*                 skeleton=nullptr;
    std::vector<Layer>              lay;
    const Animation::Sequence*      rotation=nullptr;
    int32_t                         itemUseSt     = 0;
    int32_t                         itemUseDestSt = 0;
    float                           trY=0;
    Flags                           flag=NoFlags;
    uint64_t                        lastUpdate=0;
    ComboState                      combo;
    bool                            needToUpdate = true;
    uint8_t                         hasEvents = 0;
    uint8_t                         isFlyCombined = 0;
    uint8_t                         hasTransitions = 0;

    float                           headRotX = 0, headRotY = 0;

    size_t                          numBones = 0;
    SampleStatus                    hasSamples[Resources::MAX_NUM_SKELETAL_NODES] = {};
    zenkit::AnimationSample         base      [Resources::MAX_NUM_SKELETAL_NODES] = {};
    zenkit::AnimationSample         prev      [Resources::MAX_NUM_SKELETAL_NODES] = {};
    Tempest::Matrix4x4              tr        [Resources::MAX_NUM_SKELETAL_NODES] = {};
    Tempest::Matrix4x4              pos;
  };
