#include "pose.h"

#include "skeleton.h"
#include "game/serialize.h"
#include "world/npc.h"
#include "world/world.h"
#include "animmath.h"

#include <cmath>

using namespace Tempest;

void Pose::save(Serialize &fout) {
  uint8_t sz=uint8_t(lay.size());
  fout.write(sz);
  for(auto& i:lay) {
    fout.write(i.seq->name,i.sAnim,i.bs);
    }
  fout.write(lastUpdate);
  fout.write(comboLen);
  fout.write(rotation ? rotation->name : "");
  fout.write(itemUse  ? itemUse->name  : "");
  }

void Pose::load(Serialize &fin,const AnimationSolver& solver) {
  std::string name;
  uint8_t sz=uint8_t(lay.size());

  fin.read(sz);
  lay.resize(sz);
  for(auto& i:lay) {
    fin.read(name,i.sAnim,i.bs);
    i.seq = solver.solveFrm(name.c_str());
    }
  fin.read(lastUpdate);
  if(fin.version()>=3)
    fin.read(comboLen);
  removeIf(lay,[](const Layer& l){
    return l.seq==nullptr;
    });
  fin.read(name);
  for(auto& i:lay) {
    if(i.seq->name==name)
      rotation = i.seq;
    }
  fin.read(name);
  for(auto& i:lay) {
    if(i.seq->name==name)
      itemUse = i.seq;
    }
  }

void Pose::setFlags(Pose::Flags f) {
  flag = f;
  }

BodyState Pose::bodyState() const {
  uint32_t b = BS_NONE;
  for(auto& i:lay)
    b = std::max(b,uint32_t(i.bs&(BS_MAX | BS_FLAG_MASK)));
  return BodyState(b);
  }

void Pose::setSkeleton(const Skeleton* sk) {
  if(skeleton==sk)
    return;
  skeleton = sk;
  if(skeleton!=nullptr)
    tr = skeleton->tr; else
    tr.clear();
  base = tr;
  for(size_t i=0;i<base.size() && i<skeleton->nodes.size();++i)
    base[i] = skeleton->nodes[i].tr;

  trY = skeleton->rootTr[1];

  if(lay.size()>0) //TODO
    Log::d("WARNING: ",__func__," animation adjustment not implemented");
  lay.clear();
  }

bool Pose::startAnim(const AnimationSolver& solver, const Animation::Sequence *sq, BodyState bs,
                     bool force, uint64_t tickCount) {
  if(sq==nullptr)
    return false;

  if(bs==BS_ITEMINTERACT && itemUse!=nullptr)
    return false;

  for(auto& i:lay)
    if(i.seq->layer==sq->layer) {
      const bool hasNext   = (!i.seq->next.empty() && i.seq->animCls!=Animation::Loop);
      const bool finished  = i.seq->isFinished(tickCount-i.sAnim,comboLen) && !hasNext;
      const bool interrupt = force || i.seq->canInterrupt();
      if(i.seq==sq && i.bs==bs && (interrupt || finished))
        return true;
      if(!interrupt && !finished)
        return false;
      char tansition[256]={};
      const Animation::Sequence* tr=nullptr;
      if(i.seq==itemUse) {
        if(i.seq->shortName==nullptr)
          return false;
        std::snprintf(tansition,sizeof(tansition),"T_%s_2_STAND",i.seq->shortName);
        tr = solver.solveFrm(tansition);
        }
      if(i.seq->shortName!=nullptr && sq->shortName!=nullptr) {
        std::snprintf(tansition,sizeof(tansition),"T_%s_2_%s",i.seq->shortName,sq->shortName);
        tr = solver.solveFrm(tansition);
        }
      if(tr==nullptr) {
        std::snprintf(tansition,sizeof(tansition),"T_STAND_2_%s",sq->shortName);
        tr = solver.solveFrm(tansition);
        }
      if(tr==nullptr && i.seq->shortName!=nullptr && sq->isIdle()) {
        std::snprintf(tansition,sizeof(tansition),"T_%s_2_STAND",i.seq->shortName);
        tr = solver.solveFrm(tansition);
        }
      onRemoveLayer(i);
      i.seq     = tr ? tr : sq;
      i.sAnim   = tickCount;
      i.bs      = bs;
      if(bs==BS_ITEMINTERACT)
        itemUse = i.seq;
      return true;
      }
  addLayer(sq,bs,tickCount);
  return true;
  }

bool Pose::stopAnim(const char *name) {
  bool done=false;
  size_t ret=0;
  for(size_t i=0;i<lay.size();++i) {
    if(name!=nullptr && lay[i].seq->name!=name) {
      if(ret!=i)
        lay[ret] = lay[i];
      ret++;
      } else {
      onRemoveLayer(lay[i]);
      done=true;
      }
    }
  lay.resize(ret);
  return done;
  }

void Pose::interrupt() {
  size_t ret=0;
  for(size_t i=0;i<lay.size();++i) {
    if(BS_FLAG_INTERRUPTABLE & lay[i].bs) {
      if(ret!=i)
        lay[ret] = lay[i];
      ret++;
      } else {
      onRemoveLayer(lay[i]);
      }
    }
  lay.resize(ret);
  }

void Pose::stopAllAnim() {
  for(auto& i:lay)
    onRemoveLayer(i);
  lay.clear();
  }

void Pose::update(AnimationSolver& solver, uint64_t tickCount) {
  if(lay.size()==0){
    zeroSkeleton();
    return;
    }

  size_t ret=0;
  bool   doSort=false;
  for(size_t i=0;i<lay.size();++i) {
    const auto& l = lay[i];
    if(l.seq->animCls==Animation::Transition && l.seq->isFinished(tickCount-l.sAnim,comboLen)) {
      auto next=getNext(solver,lay[i].seq);
      if(lay[i].seq==itemUse) {
        itemUse=next;
        }
      onRemoveLayer(lay[i]);

      if(next!=nullptr) {
        doSort         = lay[i].seq->layer!=next->layer;
        lay[i].seq     = next;
        lay[i].sAnim   = tickCount;
        ret++;
        }
      } else {
      if(ret!=i)
        lay[ret] = lay[i];
      ret++;
      }
    }
  lay.resize(ret);
  if(doSort) {
    std::sort(lay.begin(),lay.end(),[](const Layer& a,const Layer& b){
      return a.seq->layer<b.seq->layer;
      });
    }

  if(lastUpdate!=tickCount) {
    for(auto& i:lay)
      updateFrame(*i.seq,lastUpdate,i.sAnim,tickCount);
    lastUpdate = tickCount;
    mkSkeleton(*lay[0].seq);
    }
  }

void Pose::updateFrame(const Animation::Sequence &s,
                       uint64_t barrier, uint64_t sTime, uint64_t now) {
  auto&        d         = *s.data;
  const size_t numFrames = d.numFrames;
  const size_t idSize    = d.nodeIndex.size();
  if(numFrames==0 || idSize==0 || d.samples.size()%idSize!=0)
    return;

  (void)barrier;
  now = now-sTime;

  float    fpsRate = d.fpsRate;
  uint64_t frame   = uint64_t(float(now)*fpsRate);
  uint64_t frameA  = frame/1000;
  uint64_t frameB  = frame/1000+1; //next

  float    a       = float(frame%1000)/1000.f;

  if(s.animCls==Animation::Loop){
    frameA%=d.numFrames;
    frameB%=d.numFrames;
    } else {
    frameA = std::min<uint64_t>(frameA,d.numFrames-1);
    frameB = std::min<uint64_t>(frameB,d.numFrames-1);
    }

  if(s.reverse) {
    frameA = d.numFrames-1-frameA;
    frameB = d.numFrames-1-frameB;
    }

  auto* sampleA = &d.samples[size_t(frameA*idSize)];
  auto* sampleB = &d.samples[size_t(frameB*idSize)];

  for(size_t i=0;i<idSize;++i) {
    auto smp = mix(sampleA[i],sampleB[i],a);
    base[d.nodeIndex[i]] = mkMatrix(smp);
    }
  }

const Animation::Sequence* Pose::getNext(AnimationSolver &solver, const Animation::Sequence* sq) {
  if(sq->next.empty())
    return nullptr;
  return solver.solveAsc(sq->next.c_str());
  }

void Pose::addLayer(const Animation::Sequence *seq, BodyState bs, uint64_t tickCount) {
  if(seq==nullptr)
    return;
  Layer l;
  l.seq   = seq;
  l.sAnim = tickCount;
  l.bs    = bs;
  if(bs==BS_ITEMINTERACT)
    itemUse = seq;
  lay.push_back(l);
  std::sort(lay.begin(),lay.end(),[](const Layer& a,const Layer& b){
    return a.seq->layer<b.seq->layer;
    });
  }

void Pose::onRemoveLayer(Pose::Layer &l) {
  if(l.seq==rotation)
    rotation=nullptr;
  if(l.seq==itemUse)
    itemUse=nullptr;
  }

void Pose::processSfx(Npc &npc, uint64_t tickCount) {
  for(auto& i:lay)
    i.seq->processSfx(lastUpdate,i.sAnim,tickCount,npc);
  }

void Pose::processEvents(uint64_t &barrier, uint64_t now, Animation::EvCount &ev) const {
  for(auto& i:lay)
    i.seq->processEvents(barrier,i.sAnim,now,ev);
  barrier=now;
  }

ZMath::float3 Pose::animMoveSpeed(uint64_t tickCount,uint64_t dt) const {
  ZMath::float3 ret(0,0,0);
  for(auto& i:lay) {
    if(i.bs==BS_STAND)
      continue;
    return i.seq->speed(tickCount-i.sAnim,dt);
    }
  return ret;
  }

bool Pose::isDefParWindow(uint64_t tickCount) const {
  for(auto& i:lay)
    if(i.seq->isDefParWindow(tickCount-i.sAnim))
      return true;
  return false;
  }

bool Pose::isDefWindow(uint64_t tickCount) const {
  for(auto& i:lay)
    if(i.seq->isDefWindow(tickCount-i.sAnim))
      return true;
  return false;
  }

bool Pose::isDefence(uint64_t tickCount) const {
  char buf[32]={};
  static const char* alt[3]={"","_A2","_A3"};

  for(auto& i:lay) {
    if(i.seq->isDefWindow(tickCount-i.sAnim)) {
      // FIXME: seems like name check is not needed
      for(int h=1;h<=2;++h) {
        for(int v=0;v<3;++v) {
          std::snprintf(buf,sizeof(buf),"T_%dHPARADE_0%s",h,alt[v]);
          if(i.seq->name==buf)
            return true;
          }
        }
      }
    }
  return false;
  }

bool Pose::isJumpBack() const {
  char buf[32]={};
  for(auto& i:lay) {
    for(int h=1;h<=2;++h) {
      std::snprintf(buf,sizeof(buf),"T_%dHJUMPB",h);
      if(i.seq->name==buf)
        return true;
      }
    }
  return false;
  }

bool Pose::isJumpAnim() const {
  for(auto& i:lay)
    if(i.seq->isFly() &&
       i.seq->name!="S_JUMP"      && i.seq->name!="S_JUMPUP" &&
       i.seq->name!="S_JUMPUPMID" && i.seq->name!="S_JUMPUPLOW" &&
       i.seq->name!="S_FALL"      && i.seq->name!="S_FALLDN")
      return true;
  return false;
  }

bool Pose::isFlyAnim() const {
  for(auto& i:lay)
    if(i.seq->isFly())
      return true;
  return false;
  }

bool Pose::isStanding() const {
  if(lay.size()!=1 || lay[0].seq->animCls==Animation::Transition)
    return false;
  auto& s = *lay[0].seq;
  if(s.isIdle())
    return true;
  // check common idle animations
  return s.name=="S_FISTRUN"  || s.name=="S_MAGRUN"  ||
         s.name=="S_1HRUN"    || s.name=="S_BOWRUN"  ||
         s.name=="S_2HRUN"    || s.name=="S_CBOWRUN" ||
         s.name=="S_RUN"      || s.name=="S_WALK"    ||
         s.name=="S_FISTWALK" || s.name=="S_MAGWALK" ||
         s.name=="S_1HWALK"   || s.name=="S_BOWWALK" ||
         s.name=="S_2HWALK"   || s.name=="S_CBOWWALK";
  }

bool Pose::isItem() const {
  return itemUse;
  }

bool Pose::isPrehit(uint64_t now) const {
  for(auto& i:lay)
    if(i.seq->isPrehit(i.sAnim,now))
      return true;
  return false;
  }

bool Pose::isIdle() const {
  for(auto& i:lay)
    if(!i.seq->isIdle())
      return false;
  return true;
  }

bool Pose::isInAnim(const char* sq) const {
  for(auto& i:lay)
    if(i.seq->name==sq)
      return true;
  return false;
  }

bool Pose::isInAnim(const Animation::Sequence *sq) const {
  for(auto& i:lay)
    if(i.seq==sq)
      return true;
  return false;
  }

bool Pose::hasAnim() const {
  return lay.size()>0;
  }

uint64_t Pose::animationTotalTime() const {
  uint64_t ret=0;
  for(auto& i:lay)
    ret = std::max(ret,uint64_t(i.seq->totalTime()));
  return ret;
  }

const Animation::Sequence* Pose::continueCombo(const AnimationSolver &solver, const Animation::Sequence *sq, uint64_t tickCount) {
  if(sq==nullptr)
    return nullptr;

  bool breakCombo = true;
  for(auto& i:lay) {
    if(i.seq->name==sq->name)
      breakCombo = false;

    auto&    d = *i.seq->data;
    uint64_t t = tickCount-i.sAnim;
    for(size_t id=0;id+1<d.defWindow.size();id+=2) {
      if(d.defWindow[id+0]<=t && t<d.defWindow[id+1]) {
        if(i.seq->name==sq->name) {
          comboLen = uint16_t(id/2+1);
          return i.seq;
          } else {
          comboLen = 0;
          startAnim(solver,sq,i.bs,true,tickCount);
          return sq;
          }
        }
      }
    }

  if(breakCombo)
    comboLen = 0;
  return nullptr;
  }

uint32_t Pose::comboLength() const {
  return comboLen;
  }

Matrix4x4 Pose::cameraBone() const {
  size_t id=4;
  if(skeleton!=nullptr && skeleton->rootNodes.size())
    id = skeleton->rootNodes[0];
  return id<tr.size() ? tr[id] : Matrix4x4();
  }

void Pose::setRotation(const AnimationSolver &solver, Npc &npc, WeaponState fightMode, int dir) {
  const Animation::Sequence *sq = nullptr;
  if(dir==0) {
    if(rotation!=nullptr) {
      if(stopAnim(rotation->name.c_str()))
        rotation = nullptr;
      }
    return;
    }
  if(bodyState()!=BS_STAND)
    return;
  if(dir<0) {
    sq = solver.solveAnim(AnimationSolver::Anim::RotL,fightMode,npc.walkMode(),*this);
    } else {
    sq = solver.solveAnim(AnimationSolver::Anim::RotR,fightMode,npc.walkMode(),*this);
    }
  if(rotation!=nullptr) {
    if(rotation->name==sq->name)
      return;
    }
  if(sq==nullptr)
    return;
  if(startAnim(solver,sq,BS_FLAG_FREEHANDS,false,npc.world().tickCount())) {
    rotation = sq;
    return;
    }
  }

bool Pose::setAnimItem(const AnimationSolver &solver, Npc &npc, const char *scheme) {
  if(itemUse!=nullptr)
    return false;
  char T_ID_STAND_2_S0[128]={};
  std::snprintf(T_ID_STAND_2_S0,sizeof(T_ID_STAND_2_S0),"T_%s_STAND_2_S0",scheme);

  const Animation::Sequence *sq = solver.solveFrm(T_ID_STAND_2_S0);
  if(startAnim(solver,sq,BS_ITEMINTERACT,false,npc.world().tickCount())) {
    itemUse = sq;
    return true;
    }
  return false;
  }

Matrix4x4 Pose::mkBaseTranslation(const Animation::Sequence *s) {
  Matrix4x4 m;
  m.identity();

  if(base.size()==0)
    return m;

  size_t id=0;
  if(skeleton->rootNodes.size())
    id = skeleton->rootNodes[0];
  auto& b0=base[id];
  float dx=b0.at(3,0);
  float dy=0;
  float dz=b0.at(3,2);

  if((flag&NoTranslation)==NoTranslation)
    dy=b0.at(3,1);
  else if(s==nullptr || !s->isFly())
    dy=0;
  else if(s!=nullptr)
    dy=b0.at(3,1)-(s->data->translate.y);
  else
    dy=b0.at(3,1);

  m.translate(-dx,-dy,-dz);
  return m;
  }

void Pose::zeroSkeleton() {
  if(skeleton==nullptr)
    return;
  auto& nodes=skeleton->tr;
  if(nodes.size()<tr.size())
    return;

  Matrix4x4 m = mkBaseTranslation(nullptr);
  for(size_t i=0;i<tr.size();++i){
    tr[i] = m;
    tr[i].mul(nodes[i]);
    }
  }

void Pose::mkSkeleton(const Animation::Sequence &s) {
  if(skeleton==nullptr)
    return;
  Matrix4x4 m = mkBaseTranslation(&s);
  if(skeleton->ordered)
    mkSkeleton(m); else
    mkSkeleton(m,size_t(-1));
  }

void Pose::mkSkeleton(const Matrix4x4 &mt) {
  if(skeleton==nullptr)
    return;
  auto& nodes=skeleton->nodes;
  for(size_t i=0;i<nodes.size();++i){
    if(nodes[i].parent!=size_t(-1))
      continue;
    tr[i] = mt;
    tr[i].mul(base[i]);
    }
  for(size_t i=0;i<nodes.size();++i){
    if(nodes[i].parent==size_t(-1))
      continue;
    tr[i] = tr[nodes[i].parent];
    tr[i].mul(base[i]);
    }
  }

void Pose::mkSkeleton(const Tempest::Matrix4x4 &mt, size_t parent) {
  if(skeleton==nullptr)
    return;
  auto& nodes=skeleton->nodes;
  for(size_t i=0;i<nodes.size();++i){
    if(nodes[i].parent!=parent)
      continue;
    tr[i] = mt;
    tr[i].mul(base[i]);
    mkSkeleton(tr[i],i);
    }
  }

template<class T, class F>
void Pose::removeIf(T &t, F f) {
  size_t ret=0;
  for(size_t i=0;i<t.size();++i) {
    if( !f(t[i]) ) {
      if(ret!=i)
        t[ret] = t[i];
      ret++;
      }
    }
  t.resize(ret);
  }
