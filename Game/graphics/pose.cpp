#include "pose.h"

#include "skeleton.h"
#include "game/serialize.h"
#include "world/npc.h"

#include <cmath>

using namespace Tempest;

static Matrix4x4 getMatrix(float x,float y,float z,float w,
                           float px,float py,float pz) {
  float m[4][4]={};

  m[0][0] = w * w + x * x - y * y - z * z;
  m[0][1] = 2.0f * (x * y - w * z);
  m[0][2] = 2.0f * (x * z + w * y);
  m[1][0] = 2.0f * (x * y + w * z);
  m[1][1] = w * w - x * x + y * y - z * z;
  m[1][2] = 2.0f * (y * z - w * x);
  m[2][0] = 2.0f * (x * z - w * y);
  m[2][1] = 2.0f * (y * z + w * x);
  m[2][2] = w * w - x * x - y * y + z * z;
  m[3][0] = px;
  m[3][1] = py;
  m[3][2] = pz;
  m[3][3] = 1;

  return Matrix4x4(reinterpret_cast<float*>(m));
  }

static float mix(float x,float y,float a){
  return x+(y-x)*a;
  }

static float dotV4(const ZMath::float4 &q1, const ZMath::float4 &q2)  {
  return q1.x*q2.x + q1.y*q2.y + q1.z*q2.z + q1.w*q2.w;
  }

static ZMath::float4 slerp(const ZMath::float4 &q1, const ZMath::float4 &q2, float t)  {
  ZMath::float4 q3;
  float dot = dotV4(q1, q2);

  /*	dot = cos(theta)
    if (dot < 0), q1 and q2 are more than 90 degrees apart,
    so we can invert one to reduce spinning	*/
  if( dot<0 ) {
    dot = -dot;
    q3.x = -q2.x;
    q3.y = -q2.y;
    q3.z = -q2.z;
    q3.w = -q2.w;
    } else {
    q3 = q2;
    }

  if( dot<0.95f) {
    float angle = std::acos(dot);
    float s     = std::sin(angle*(1.f-t));
    float st    = std::sin(angle*t);
    float sa    = std::sin(angle);

    q3.x = (q1.x*s + q3.x*st)/sa;
    q3.y = (q1.y*s + q3.y*st)/sa;
    q3.z = (q1.z*s + q3.z*st)/sa;
    q3.w = (q1.w*s + q3.w*st)/sa;

    return q3; //(q1*sinf(angle*(1-t)) + q3*sinf(angle*t))/sinf(angle);
    } else {
    // if the angle is small, use linear interpolation
    //return lerp(q1,q3,t);
    float t1=1.f-t;
    q3.x = q1.x*t1 + q3.x*t;
    q3.y = q1.y*t1 + q3.y*t;
    q3.z = q1.z*t1 + q3.z*t;
    q3.w = q1.w*t1 + q3.w*t;

    const float l = std::sqrt(q3.x*q3.x + q3.y*q3.y + q3.z*q3.z + q3.w*q3.w);
    q3.x /= l;
    q3.y /= l;
    q3.z /= l;
    q3.w /= l;

    return q3;
    }
  }

static ZenLoad::zCModelAniSample mix(const ZenLoad::zCModelAniSample& x,const ZenLoad::zCModelAniSample& y,float a){
  ZenLoad::zCModelAniSample r;

  r.rotation   = slerp(x.rotation,y.rotation,a);

  r.position.x = mix(x.position.x,y.position.x,a);
  r.position.y = mix(x.position.y,y.position.y,a);
  r.position.z = mix(x.position.z,y.position.z,a);

  return r;
  }

Pose::Pose(const Skeleton &sk, const Animation::Sequence* sq0, const Animation::Sequence *sq1)
  :skeleton(&sk) {
  if(skeleton!=nullptr)
    tr = skeleton->tr; else
    tr.clear();
  base = tr;
  for(size_t i=0;i<base.size() && i<sk.nodes.size();++i)
    base[i] = sk.nodes[i].tr;
  trY = sk.rootTr[1];

  addLayer(sq0,0);
  addLayer(sq1,0);
  }

void Pose::save(Serialize &fout) {
  uint8_t sz=uint8_t(lay.size());
  fout.write(sz);
  for(auto& i:lay) {
    fout.write(i.seq->name,i.frame,i.sAnim);
    }
  }

void Pose::load(Serialize &fin,const AnimationSolver& solver) {
  std::string name;
  uint8_t sz=uint8_t(lay.size());

  fin.read(sz);
  lay.resize(sz);
  for(auto& i:lay) {
    fin.read(name,i.frame,i.sAnim);
    i.seq = solver.findSequence(name.c_str());
    }
  removeIf(lay,[](const Layer& l){
    return l.seq==nullptr;
    });
  }

bool Pose::startAnim(const AnimationSolver& solver, const Animation::Sequence *sq, uint64_t tickCount) {
  if(sq==nullptr)
    return false;

  for(auto& i:lay)
    if(i.seq->layer==sq->layer) {
      const bool finished  = i.seq->isFinished(tickCount-i.sAnim);
      const bool interrupt = i.seq->canInterrupt();
      if(i.seq==sq && (interrupt || finished))
        return true;
      if(!interrupt && !finished)
        return false;
      const Animation::Sequence* tr=nullptr;
      if(i.seq->shortName.size()>0 && sq->shortName.size()>0) {
        char tansition[256]={};
        std::snprintf(tansition,sizeof(tansition),"T_%s_2_%s",i.seq->shortName.c_str(),sq->shortName.c_str());
        tr = solver.findSequence(tansition);
        }
      i.seq   = tr ? tr : sq;
      i.sAnim = tickCount;
      return true;
      }
  addLayer(sq,tickCount);
  return true;
  }

void Pose::reset(const Skeleton &sk, uint64_t tickCount, const Animation::Sequence *sq0, const Animation::Sequence *sq1) {
  if(skeleton!=&sk){
    skeleton = &sk;
    if(skeleton!=nullptr)
      tr = skeleton->tr; else
      tr.clear();
    base = tr;
    for(size_t i=0;i<base.size() && i<sk.nodes.size();++i)
      base[i] = sk.nodes[i].tr;
    trY = sk.rootTr[1];
    }
  lay.clear();
  addLayer(sq0,tickCount);
  addLayer(sq1,tickCount);
  }

void Pose::update(uint64_t tickCount) {
  if(lay.size()==0){
    zeroSkeleton();
    return;
    }

  bool change=false;
  for(auto& i:lay)
    change |= update(*i.seq,tickCount-i.sAnim,i.frame);
  size_t ret=0;
  for(size_t i=0;i<lay.size();++i) {
    const auto& l = lay[i];
    if((l.seq->flags & Animation::Idle) || !l.seq->isFinished(tickCount-l.sAnim)) {
      if(ret!=i)
        lay[ret] = lay[i];
      ret++;
      } else {
      if(lay[i].seq!=nullptr && lay[i].seq->next!=nullptr) {
        lay[i].seq = lay[i].seq->next;
        ret++;
        }
      }
    }
  if(ret==0)
    ret=1;
  lay.resize(ret);

  if(!change)
    return;
  mkSkeleton(*lay[0].seq);
  }

bool Pose::update(const Animation::Sequence &s, uint64_t dt, uint64_t& fr) {
  uint64_t nfr = uint64_t(s.data->fpsRate*dt);
  if(nfr==fr)
    return false;
  fr = nfr;
  updateFrame(s,fr);
  return true;
  }

void Pose::updateFrame(const Animation::Sequence &s, uint64_t fr) {
  float    a      = (fr%1000)/1000.f;
  uint64_t frameA = (fr/1000  );
  uint64_t frameB = (fr/1000+1);

  if(s.animCls==Animation::Loop){
    frameA%=s.data->numFrames;
    frameB%=s.data->numFrames;
    } else {
    frameA = std::min<uint64_t>(frameA,s.data->numFrames-1);
    frameB = std::min<uint64_t>(frameB,s.data->numFrames-1);
    }

  if(s.reverse) {
    frameA = s.data->numFrames-1-frameA;
    frameB = s.data->numFrames-1-frameB;
    }

  auto& d = *s.data;
  const size_t idSize=d.nodeIndex.size();
  if(idSize==0 || d.samples.size()%idSize!=0)
    return;

  auto* sampleA = &d.samples[size_t(frameA*idSize)];
  auto* sampleB = &d.samples[size_t(frameB*idSize)];

  for(size_t i=0;i<idSize;++i) {
    auto  smp = mix(sampleA[i],sampleB[i],a);
    auto& pos = smp.position;
    auto& rot = smp.rotation;

    base[d.nodeIndex[i]] = getMatrix(rot.x,rot.y,rot.z,rot.w,pos.x,pos.y,pos.z);
    }
  }

void Pose::addLayer(const Animation::Sequence *seq, uint64_t tickCount) {
  if(seq==nullptr)
    return;
  Layer l;
  l.seq   = seq;
  l.frame = uint64_t(-1);
  l.sAnim = tickCount;
  lay.push_back(l);
  std::sort(lay.begin(),lay.end(),[](const Layer& a,const Layer& b){
    return a.seq->layer<b.seq->layer;
    });
  }

void Pose::emitSfx(Npc &npc, uint64_t tickCount) {
  for(auto& i:lay)
    i.seq->emitSfx(npc,tickCount-i.sAnim,i.frame);
  }

void Pose::processEvents(uint64_t &barrier, uint64_t now, Animation::EvCount &ev) const {
  for(auto& i:lay)
    i.seq->processEvents(barrier,i.sAnim,now,ev);
  }

ZMath::float3 Pose::animMoveSpeed(uint64_t tickCount,uint64_t dt) const {
  if(lay.size()>0)
    return lay[0].seq->speed(tickCount-lay[0].sAnim,dt);
  return ZMath::float3(0,0,0);
  }

bool Pose::isParWindow(uint64_t tickCount) const {
  for(auto& i:lay)
    if(i.seq->isParWindow(tickCount-i.sAnim))
      return true;
  return false;
  }

bool Pose::isAtackFinished(uint64_t tickCount) const {
  for(auto& i:lay)
    if(i.seq->isAtackFinished(tickCount-i.sAnim))
      return true;
  return false;
  }

bool Pose::isFlyAnim() const {
  for(auto& i:lay)
    if(i.seq->isFly())
      return true;
  return false;
  }

bool Pose::isInAnim(const char* sq) const {
  for(auto& i:lay)
    if(i.seq->name==sq)
      return true;
  return false;
  }

Matrix4x4 Pose::cameraBone() const {
  size_t id=4;
  if(skeleton->rootNodes.size())
    id = skeleton->rootNodes[0];
  return id<tr.size() ? tr[id] : Matrix4x4();
  }

void Pose::zeroSkeleton() {
  auto& nodes=skeleton->tr;
  if(nodes.size()<tr.size())
    return;

  Matrix4x4 m;
  m.identity();
  if(base.size()) {
    size_t id=0;
    if(skeleton->rootNodes.size())
      id = skeleton->rootNodes[0];
    auto& b0=base[id];
    float dx=b0.at(3,0);//-s.translate.x;
    float dy=b0.at(3,1);//-s.translate.y;
    float dz=b0.at(3,2);//-s.translate.z;
    //dy=0;
    m.translate(-dx,-dy/2,-dz);
    }

  for(size_t i=0;i<tr.size();++i){
    tr[i] = m;
    tr[i].mul(nodes[i]);
    }
  }

void Pose::mkSkeleton(const Animation::Sequence &s) {
  Matrix4x4 m;
  m.identity();
  if(base.size()) {
    size_t id=0;
    if(skeleton->rootNodes.size())
      id = skeleton->rootNodes[0];
    auto& b0=base[id];
    float dx=b0.at(3,0);//-s.translate.x;
    float dy=b0.at(3,1)-s.data->translate.y;
    float dz=b0.at(3,2);//-s.translate.z;
    if(!s.isFly())
      dy=0;
    m.translate(-dx,-dy,-dz);
    }

  if(skeleton->ordered)
    mkSkeleton(m); else
    mkSkeleton(m,size_t(-1));
  }

void Pose::mkSkeleton(const Matrix4x4 &mt) {
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
