#include "pfxbucket.h"

#include "graphics/mesh/submesh/pfxemittermesh.h"
#include "graphics/shaders.h"
#include "pfxobjects.h"
#include "particlefx.h"

#include "world/objects/npc.h"

using namespace Tempest;

static uint64_t ppsDiff(const ParticleFx& decl, bool loop, uint64_t time0, uint64_t time1) {
  if(time1<=time0)
    return 0;
  if(decl.prefferedTime==0 && time0==0 && !loop)
    return uint64_t(decl.ppsValue*1000.f);
  if(!loop) {
    time0 = std::min(decl.prefferedTime,time0);
    time1 = std::min(decl.prefferedTime,time1);
    }
  const float pps     = decl.ppsScale(time1)*decl.ppsValue;
  uint64_t    emitted0 = uint64_t(pps*float(time0)/1000.f);
  uint64_t    emitted1 = uint64_t(pps*float(time1)/1000.f);
  return emitted1-emitted0;
  }

float PfxBucket::ParState::lifeTime() const {
  return 1.f-life/float(maxLife);
  }

std::mt19937 PfxBucket::rndEngine;

bool PfxBucket::Draw::isEmpty() const {
  return (pfxGpu.byteSize()==0);
  }

PfxBucket::PfxBucket(const ParticleFx &decl, PfxObjects& parent, const SceneGlobals& scene, VisualObjects& visual)
  :decl(decl), parent(parent), visual(visual)  {
  uint64_t lt      = decl.maxLifetime();
  uint64_t pps     = uint64_t(std::ceil(decl.maxPps()));
  uint64_t reserve = (lt*pps+1000-1)/1000;
  blockSize        = size_t(reserve);
  if(blockSize==0)
    blockSize=1;

  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i) {
    auto& item = this->item[i];
    if(decl.visMaterial.tex==nullptr)
      continue;

    item.pMain   = Shaders::inst().materialPipeline(decl.visMaterial, DrawCommands::Pfx, Shaders::T_Main, false);
    item.pShadow = Shaders::inst().materialPipeline(decl.visMaterial, DrawCommands::Pfx, Shaders::T_Shadow, false);
    }

  if(decl.hasTrails()) {
    maxTrlTime = uint64_t(decl.trlFadeSpeed*1000.f);

    Material mat = decl.visMaterial;
    mat.tex = decl.trlTexture;

    for(size_t i=0; i<Resources::MaxFramesInFlight; ++i) {
      auto& item = this->itemTrl[i];
      if(mat.tex==nullptr)
        continue;

      item.pMain   = Shaders::inst().materialPipeline(mat, DrawCommands::Pfx, Shaders::T_Main, false);
      item.pShadow = Shaders::inst().materialPipeline(mat, DrawCommands::Pfx, Shaders::T_Shadow, false);
      }
    }
  }

PfxBucket::~PfxBucket() {
  }

bool PfxBucket::isEmpty() const {
  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i) {
    if(!item[i].isEmpty())
      return false;
    //if(!itemTrl[i].isEmpty())
    //  return false;
    }
  return impl.size()==0;
  }

size_t PfxBucket::allocBlock() {
  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i)
    forceUpdate[i] = true;

  for(size_t i=0;i<block.size();++i) {
    if(!block[i].allocated) {
      block[i].allocated = true;
      block[i].timeTotal = 0;
      return i;
      }
    }

  block.emplace_back();

  Block& b = block.back();
  b.allocated = true;
  b.offset    = particles.size();
  b.timeTotal = 0;

  particles.resize(particles.size()+blockSize);
  pfxCpu   .resize(particles.size());

  for(size_t i=0; i<blockSize; ++i)
    particles[b.offset+i].life = 0;
  return block.size()-1;
  }

void PfxBucket::freeBlock(size_t& i) {
  if(i==size_t(-1))
    return;
  auto& b = block[i];
  assert(b.count==0);

  pfxCpu[b.offset].size = Vec3();

  b.allocated = false;
  i = size_t(-1);
  }

PfxBucket::Block& PfxBucket::getBlock(ImplEmitter &e) {
  if(e.block==size_t(-1)) {
    e.block = allocBlock();
    auto& p = block[e.block];
    p.pos   = e.pos;
    }
  return block[e.block];
  }

PfxBucket::Block& PfxBucket::getBlock(PfxEmitter& e) {
  return getBlock(impl[e.id]);
  }

size_t PfxBucket::allocEmitter() {
  for(size_t i=0; i<impl.size(); ++i) {
    auto& b = impl[i];
    if(b.st==S_Free) {
      b.st = S_Inactive;
      return i;
      }
    }
  impl.emplace_back();
  auto& e = impl.back();
  e.block = size_t(-1); // no backup memory
  e.st    = S_Inactive;
  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i)
    forceUpdate[i] = true;

  return impl.size()-1;
  }

void PfxBucket::freeEmitter(size_t& id) {
  auto& v = impl[id];
  if(v.block!=size_t(-1)) {
    auto& b = getBlock(v);
    v.st    = b.count==0 ? S_Free : S_Fade;
    if(b.count==0)
      freeBlock(v.block);
    } else {
    v.st    = S_Free;
    }
  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i)
    forceUpdate[i] = true;
  v.next.reset();
  id = size_t(-1);
  shrink();
  }

bool PfxBucket::shrink() {
  while(impl.size()>0) {
    auto& b = impl.back();
    if(b.st!=S_Free)
      break;
    impl.pop_back();
    }
  while(block.size()>0) {
    auto& b = block.back();
    if(b.allocated)
      break;
    block.pop_back();
    }
  if(particles.size()!=block.size()*blockSize) {
    particles.resize(block.size()*blockSize);
    pfxCpu   .resize(particles.size());
    return true;
    }
  return false;
  }

float PfxBucket::randf() {
  return float(rndEngine()%10000)/10000.f;
  }

float PfxBucket::randf(float base, float var) {
  return (2.f*randf()-1.f)*var + base;
  }

void PfxBucket::init(PfxBucket::Block& block, ImplEmitter& emitter, size_t particle) {
  auto& p   = particles[particle];

  p.life    = uint16_t(randf(decl.lspPartAvg,decl.lspPartVar));
  p.maxLife = p.life;

  // TODO: pfx.shpDistribType, pfx.shpDistribWalkSpeed;
  switch(decl.shpType) {
    case ParticleFx::EmitterType::Point:{
      p.pos = Vec3();
      break;
      }
    case ParticleFx::EmitterType::Line:{
      float at = randf();
      p.pos = Vec3(at,at,at);
      break;
      }
    case ParticleFx::EmitterType::Box:{
      if(decl.shpIsVolume) {
        p.pos = Vec3(randf()*2.f-1.f,
                     randf()*2.f-1.f,
                     randf()*2.f-1.f);
        p.pos*=0.5;
        } else {
        // TODO
        p.pos = Vec3(randf()*2.f-1.f,
                     randf()*2.f-1.f,
                     randf()*2.f-1.f);
        p.pos*=0.5;
        }
      break;
      }
    case ParticleFx::EmitterType::Sphere:{
      float theta = float(2.0*M_PI)*randf();
      float phi   = std::acos(1.f - 2.f * randf());
      p.pos = Vec3(std::sin(phi) * std::cos(theta),
                   std::sin(phi) * std::sin(theta),
                   std::cos(phi));
      //p.pos*=0.5;
      if(decl.shpIsVolume)
        p.pos*=randf();
      break;
      }
    case ParticleFx::EmitterType::Circle:{
      float a = float(2.0*M_PI)*randf();
      p.pos = Vec3(std::sin(a),
                   0,
                   std::cos(a));
      //p.pos*=0.5;
      if(decl.shpIsVolume)
        p.pos = p.pos*std::sqrt(randf());
      break;
      }
    case ParticleFx::EmitterType::Mesh:{
      p.pos = Vec3();
      auto mesh = (emitter.mesh!=nullptr) ? emitter.mesh : decl.shpMesh;
      auto pose = (emitter.mesh!=nullptr) ? emitter.pose : nullptr;
      if(mesh!=nullptr) {
        auto pos = mesh->randCoord(randf(),pose);
        pos -= emitter.pos;
        p.pos = emitter.direction[0]*pos.x +
                emitter.direction[1]*pos.y +
                emitter.direction[2]*pos.z;
        }
      break;
      }
    }

  if(decl.shpType!=ParticleFx::EmitterType::Point &&
     decl.shpType!=ParticleFx::EmitterType::Mesh) {
    Vec3 dim = decl.shpDim*decl.shpScale(block.timeTotal);
    p.pos.x*=dim.x;
    p.pos.y*=dim.y;
    p.pos.z*=dim.z;
    }

  switch(decl.shpFOR) {
    case ParticleFx::Frame::Object:
    case ParticleFx::Frame::Node: {
      p.pos += emitter.direction[0]*decl.shpOffsetVec.x +
               emitter.direction[1]*decl.shpOffsetVec.y +
               emitter.direction[2]*decl.shpOffsetVec.z;
      break;
      }
    case ParticleFx::Frame::World: {
      p.pos += decl.shpOffsetVec;
      break;
      }
    }

  switch(decl.dirMode) {
    case ParticleFx::Dir::Rand: {
      float dy    = 1.f - 2.f * randf();
      float sn    = std::sqrt(1-dy*dy);
      float theta = float(2.0*M_PI)*randf();
      float dx    = sn * std::cos(theta);
      float dz    = sn * std::sin(theta);

      p.dir       = Vec3(dx,dy,dz);
      break;
      }
    case ParticleFx::Dir::Dir: {
      float dirAngleHeadVar = decl.dirAngleHeadVar;
      float dirAngleElevVar = decl.dirAngleElevVar;

      // HACK: STARGATE_PARTICLES
      if(decl.dirAngleHeadVar>=180)
        dirAngleHeadVar = 0;
      if(decl.dirAngleElevVar>=180 )
        dirAngleElevVar = 0;

      float head = (90+randf(decl.dirAngleHead,dirAngleHeadVar))*float(M_PI)/180.f;
      float elev = (   randf(decl.dirAngleElev,dirAngleElevVar))*float(M_PI)/180.f;

      float dx = std::cos(elev) * std::cos(head);
      float dy = std::sin(elev);
      float dz = std::cos(elev) * std::sin(head);

      switch(decl.dirFOR) {
        case ParticleFx::Frame::Object:
        case ParticleFx::Frame::Node: {
          p.dir = emitter.direction[0]*dx +
                  emitter.direction[1]*dy +
                  emitter.direction[2]*dz;
          break;
          }
        case ParticleFx::Frame::World: {
          p.dir = Vec3(dx,dy,dz);
          break;
          }
        }
      break;
      }
    case ParticleFx::Dir::Target:
      Vec3 targetPos = emitter.pos;
      switch(decl.dirModeTargetFOR) {
        case ParticleFx::Frame::World:
          targetPos = decl.dirModeTargetPos;
          break;
        case ParticleFx::Frame::Node:
        case ParticleFx::Frame::Object: {
          if(emitter.targetNpc!=nullptr) {
            // MFX_WINDFIST_CAST
            auto mt = emitter.targetNpc->transform();
            mt.project(targetPos);
            } else {
            // IRRLICHT_DIE
            targetPos = emitter.pos;
            }
          break;
          }
        }
      p.dir += targetPos - (emitter.pos+p.pos);
      break;
    }

  if(!decl.useEmittersFOR)
    p.pos += emitter.pos;

  auto l = p.dir.length();
  if(l!=0.f) {
    float velocity = randf(decl.velAvg,decl.velVar);
    p.dir = p.dir*velocity/l;
    }
  }

void PfxBucket::finalize(size_t particle) {
  particles[particle] = {};
  pfxCpu   [particle] = {};
  }

void PfxBucket::tick(Block& sys, ImplEmitter& emitter, size_t particle, uint64_t dt) {
  ParState& ps = particles[particle+sys.offset];
  if(ps.life==0)
    return;

  if(ps.life<=dt){
    ps.life = 0;
    sys.count--;
    finalize(particle+sys.offset);
    return;
    }

  const float dtF = float(dt);

  // eval particle
  ps.life  = uint16_t(ps.life-dt);
  ps.pos  += ps.dir*dtF;
  ps.dir  += decl.flyGravity*dtF;

  if(maxTrlTime!=0)
    tickTrail(ps,emitter,dt);
  }

void PfxBucket::tickTrail(ParState& ps, ImplEmitter& emitter, uint64_t dt) {
  for(auto& i:ps.trail)
    i.time+=dt;

  Trail tx;
  if(decl.useEmittersFOR)
    tx.pos = ps.pos + emitter.pos; else
    tx.pos = ps.pos;

  if(ps.trail.size()==0) {
    ps.trail.push_back(tx);
    }
  else if(ps.trail.back().pos!=tx.pos) {
    bool extrude = false;
    if(false && ps.trail.size()>1) {
      auto u = tx.pos              - ps.trail[ps.trail.size()-2].pos;
      auto v = ps.trail.back().pos - ps.trail[ps.trail.size()-2].pos;
      if(std::abs(Vec3::dotProduct(u,v)-u.length()*v.length()) < 0.001f)
        extrude = true;
      }
    if(extrude)
      ps.trail.back() = tx; else
      ps.trail.push_back(tx);
    }
  else {
    ps.trail.back().time = 0;
    }

  for(size_t rm=0; rm<=ps.trail.size(); ++rm) {
    if(rm==ps.trail.size() || ps.trail[rm].time<maxTrlTime) {
      ps.trail.erase(ps.trail.begin(),ps.trail.begin()+int(rm));
      break;
      }
    }
  }

void PfxBucket::tick(uint64_t dt, const Vec3& viewPos) {
  if(decl.isDecal()) {
    implTickDecals(dt,viewPos);
    return;
    }
  implTickCommon(dt,viewPos);
  }

void PfxBucket::implTickCommon(uint64_t dt, const Vec3& viewPos) {
  bool doShrink = false;
  for(auto& emitter:impl) {
    if(emitter.st==S_Free)
      continue;

    const auto dp     = emitter.pos-viewPos;
    const bool nearby = (dp.quadLength()<PfxObjects::viewRage*PfxObjects::viewRage);

    if(emitter.next==nullptr && decl.ppsCreateEm!=nullptr && emitter.waitforNext<dt && emitter.st==S_Active) {
      emitter.next.reset(new PfxEmitter(parent,decl.ppsCreateEm));
      auto& e = *emitter.next;
      e.setPosition(emitter.pos.x,emitter.pos.y,emitter.pos.z);
      e.setActive(true);
      e.setLooped(emitter.isLoop);
      }

    if(emitter.waitforNext>=dt)
      emitter.waitforNext-=dt;

    if(emitter.block!=size_t(-1)) {
      auto& p = getBlock(emitter);
      if(p.count>0) {
        for(size_t i=0;i<blockSize;++i)
          tick(p,emitter,i,dt);
        if(p.count==0 && (emitter.st==S_Fade || !nearby)) {
          // free mem
          freeBlock(emitter.block);
          if(emitter.st==S_Fade)
            emitter.st = S_Free;
          doShrink = true;
          continue;
          }
        }
      }

    if(emitter.st==S_Active && nearby) {
      auto& p = getBlock(emitter);
      auto dE = ppsDiff(decl,emitter.isLoop,p.timeTotal,p.timeTotal+dt);
      tickEmit(p,emitter,dE);
      }

    if(emitter.block!=size_t(-1)) {
      auto& p = getBlock(emitter);
      p.timeTotal+=dt;
      }
    }

  if(doShrink)
    shrink();
  }

void PfxBucket::implTickDecals(uint64_t, const Vec3&) {
  for(auto& emitter:impl) {
    if(emitter.st==S_Free)
      continue;

    auto& p = getBlock(emitter);
    if(emitter.st==S_Active || emitter.st==S_Inactive) {
      if(p.count==0)
        tickEmit(p,emitter,1);
      } else
    if(emitter.st==S_Fade) {
      for(size_t i=0; i<blockSize; ++i)
        particles[p.offset+i].life = 0;
      p.count = 0;
      freeBlock(emitter.block);
      emitter.st = S_Free;
      }
    }
  }

void PfxBucket::tickEmit(Block& p, ImplEmitter& emitter, uint64_t emited) {
  size_t lastI = 0;
  for(size_t id=1; emited>0; ++id) {
    const size_t i  = id%blockSize;
    ParState&    ps = particles[i+p.offset];
    if(ps.life==0) { // free slot
      --emited;
      lastI = i;
      init(p,emitter,i+p.offset);
      if(ps.life==0)
        continue;
      p.count++;
      } else {
      // out of slots
      if(lastI==i)
        return;
      }
    }
  }

void PfxBucket::buildSsbo() {
  buildSsboTrails();

  auto  colorS          = decl.visTexColorStart;
  auto  colorE          = decl.visTexColorEnd;
  auto  visSizeStart    = decl.visSizeStart;
  auto  visSizeEndScale = decl.visSizeEndScale;
  auto  visAlphaStart   = decl.visAlphaStart;
  auto  visAlphaEnd     = decl.visAlphaEnd;
  auto  visAlphaFunc    = decl.visMaterial.alpha;

  for(auto& p:block) {
    if(p.count==0)
      continue;

    for(size_t pId=0; pId<blockSize; ++pId) {
      ParState& ps = particles[pId+p.offset];
      auto&     px = pfxCpu   [pId+p.offset];

      if(ps.life==0) {
        px.size = Vec3();
        continue;
        }

      const float a     = ps.lifeTime();
      const Vec3  cl    = colorS*(1.f-a)        + colorE*a;
      const float clA   = visAlphaStart*(1.f-a) + visAlphaEnd*a;

      const float scale = 1.f*(1.f-a) + a*visSizeEndScale;
      const float szX   = visSizeStart.x*scale;
      const float szY   = visSizeStart.y*scale;
      const float szZ   = 0.1f*((szX+szY)*0.5f);

      struct Color {
        uint8_t r=255;
        uint8_t g=255;
        uint8_t b=255;
        uint8_t a=255;
        } color;

      if(visAlphaFunc==Material::AlphaFunc::AdditiveLight) {
        color.r = uint8_t(cl.x*clA);
        color.g = uint8_t(cl.y*clA);
        color.b = uint8_t(cl.z*clA);
        color.a = uint8_t(255);
        } else {
        color.r = uint8_t(cl.x);
        color.g = uint8_t(cl.y);
        color.b = uint8_t(cl.z);
        color.a = uint8_t(clA*255);
        }
      uint32_t colorU32;
      std::memcpy(&colorU32,&color,4);
      buildBilboard(px,p,ps, colorU32, szX,szY,szZ);
      }
    }
  }

void PfxBucket::buildSsboTrails() {
  if(!decl.hasTrails())
    return;

  trlCpu.reserve(trlCpu.size());
  trlCpu.clear();

  for(auto& p:particles) {
    if(p.life==0)
      continue;
    if(p.trail.size()<2)
      continue;

    float maxT = float(std::min(maxTrlTime,p.trail[0].time));
    for(size_t r=1; r<p.trail.size(); ++r) {
      PfxState st;
      buildTrailSegment(st,p.trail[r-1],p.trail[r],maxT);
      trlCpu.push_back(st);
      }
    }
  }

void PfxBucket::buildBilboard(PfxState& v, const Block& p, const ParState& ps, const uint32_t color,
                              float szX, float szY, float szZ) {
  if(decl.useEmittersFOR)
    v.pos = ps.pos + p.pos; else
    v.pos = ps.pos;

  v.size  = Vec3(szX,szY,szZ);
  v.color = color;
  v.bits0 = 0;
  v.bits0 |= uint32_t(decl.visZBias ? 1 : 0);
  v.bits0 |= uint32_t(decl.visTexIsQuadPoly ? 1 : 0) << 1;
  v.bits0 |= uint32_t(decl.visYawAlign ? 1 : 0) << 2;
  v.bits0 |= uint32_t(0) << 3; // TODO: trails
  v.bits0 |= uint32_t(decl.visOrientation) << 4;
  v.dir   = ps.dir;
  }

void PfxBucket::buildTrailSegment(PfxState& v, const Trail& a, const Trail& b, float maxT) {
  float    tA  = 1.f - float(a.time)/maxT;
  float    tB  = 1.f - float(b.time)/maxT;

  uint32_t clA = mkTrailColor(tA);
  uint32_t clB = mkTrailColor(tB);

  v.pos    = a.pos;
  v.color  = clA;
  v.size   = Vec3(decl.trlWidth,tA,tB);
  v.bits0  = uint32_t(1) << 3;
  v.dir    = b.pos  - a.pos;
  v.colorB = clB;
  }

uint32_t PfxBucket::mkTrailColor(float clA) const {
  struct Color {
    uint8_t r=255;
    uint8_t g=255;
    uint8_t b=255;
    uint8_t a=255;
    } color;

  clA = std::max(0.f, std::min(clA, 1.f));

  if(decl.visMaterial.alpha==Material::AlphaFunc::AdditiveLight) {
    color.r = uint8_t(255*clA);
    color.g = uint8_t(255*clA);
    color.b = uint8_t(255*clA);
    color.a = uint8_t(255);
    }
  else if(decl.visMaterial.alpha==Material::AlphaFunc::Transparent) {
    color.r = 255;
    color.g = 255;
    color.b = 255;
    color.a = uint8_t(clA*255);
    }
  uint32_t cl;
  std::memcpy(&cl,&color,sizeof(cl));
  return cl;
  }

void PfxBucket::preFrameUpdate(const SceneGlobals& scene, uint8_t fId) {
  auto& device = Resources::device();

  if(item[fId].pMain!=nullptr || item[fId].pShadow!=nullptr) {
    // hidden staging under the hood
    auto& ssbo = item[fId].pfxGpu;
    if(pfxCpu.size()*sizeof(PfxBucket::PfxState)!=ssbo.byteSize()) {
      auto heap = decl.isDecal() ? BufferHeap::Device : BufferHeap::Upload;
      ssbo = device.ssbo(heap,pfxCpu);
      } else {
      if(!decl.isDecal() || forceUpdate[fId]) {
        ssbo.update(pfxCpu);
        forceUpdate[fId] = false;
        }
      }
    }

  if(itemTrl[fId].pMain!=nullptr || itemTrl[fId].pShadow!=nullptr) {
    auto& ssbo = itemTrl[fId].pfxGpu;
    if(trlCpu.size()*sizeof(PfxBucket::PfxState)!=ssbo.byteSize()) {
      ssbo = device.ssbo(BufferHeap::Upload, trlCpu);
      } else {
      ssbo.update(trlCpu);
      }
    }
  }

void PfxBucket::drawGBuffer(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const SceneGlobals& scene, uint8_t fId) {
  for(auto i:{Material::Solid, Material::AlphaTest}) {
    drawCommon(cmd, scene, item[fId],    SceneGlobals::V_Main, i, false);
    drawCommon(cmd, scene, itemTrl[fId], SceneGlobals::V_Main, i, true);
    }
  }

void PfxBucket::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const SceneGlobals& scene, uint8_t fId, int layer) {
  auto view = SceneGlobals::VisCamera(SceneGlobals::V_Shadow0 + layer);
  for(auto i:{Material::Solid, Material::AlphaTest}) {
    drawCommon(cmd, scene, item[fId],    view, i, false);
    drawCommon(cmd, scene, itemTrl[fId], view, i, true);
    }
  }

void PfxBucket::drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const SceneGlobals& scene, uint8_t fId) {
  for(auto i:{/*Material::Water, */Material::Multiply, Material::Multiply2, Material::Transparent, Material::AdditiveLight, Material::Ghost}) {
    drawCommon(cmd, scene, item[fId],    SceneGlobals::V_Main, i, false);
    drawCommon(cmd, scene, itemTrl[fId], SceneGlobals::V_Main, i, true);
    }
  }

void PfxBucket::drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const SceneGlobals& scene, const Draw& itm,
                           SceneGlobals::VisCamera view, Material::AlphaFunc func, bool trl) {
  if(itm.pfxGpu.isEmpty())
    return;

  if(decl.visMaterial.alpha!=func)
    return;

  const RenderPipeline* pso = nullptr;
  switch(view) {
    case SceneGlobals::V_Shadow0:
    case SceneGlobals::V_Shadow1:
    case SceneGlobals::V_Vsm:
      pso = itm.pShadow;
      break;
    case SceneGlobals::V_Main:
      pso = itm.pMain;
      break;
    case SceneGlobals::V_HiZ:
    case SceneGlobals::V_Count:
      break;
    }
  if(pso==nullptr)
    return;

  auto& mat = decl.visMaterial;
  if(view==SceneGlobals::V_Main || mat.isTextureInShadowPass()) {
    auto smp = SceneGlobals::isShadowView(SceneGlobals::VisCamera(view)) ? Sampler::trillinear() : Sampler::anisotrophy();
    if(trl) {
      cmd.setBinding(L_Diffuse, *decl.trlTexture);
      }
    else if(mat.hasFrameAnimation()) {
      auto frame = size_t((itm.timeShift+scene.tickCount)/mat.texAniFPSInv);
      auto t     = mat.frames[frame%mat.frames.size()];
      cmd.setBinding(L_Diffuse, *t);
      }
    else {
      cmd.setBinding(L_Diffuse, *mat.tex);
      }
    cmd.setBinding(L_Sampler, smp);
    }

  if(view==SceneGlobals::V_Main && mat.isShadowmapRequired()) {
    cmd.setBinding(L_Shadow0,  *scene.shadowMap[0],Resources::shadowSampler());
    cmd.setBinding(L_Shadow1,  *scene.shadowMap[1],Resources::shadowSampler());
    }

  if(view==SceneGlobals::V_Main && mat.isSceneInfoRequired()) {
    cmd.setBinding(L_SceneClr, *scene.sceneColor, Sampler::bilinear(ClampMode::MirroredRepeat));
    cmd.setBinding(L_GDepth,   *scene.sceneDepth, Sampler::nearest (ClampMode::MirroredRepeat));
    }

  cmd.setBinding(L_Scene, scene.uboGlobal[view]);
  cmd.setBinding(L_Pfx,   itm.pfxGpu);
  cmd.setPipeline(*pso);
  cmd.draw(nullptr, 0, 6, 0, itm.pfxGpu.byteSize()/sizeof(PfxState));
  }
