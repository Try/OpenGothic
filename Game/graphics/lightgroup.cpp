#include "lightgroup.h"

#include "bounds.h"
#include "graphics/rendererstorage.h"
#include "graphics/sceneglobals.h"
#include "utils/gthfont.h"
#include "utils/workers.h"
#include "utils/dbgpainter.h"

#include "world/world.h"

using namespace Tempest;

LightGroup::Light::Light(LightGroup::Light&& oth):light(oth.light), id(oth.id) {
  oth.light = nullptr;
  }

LightGroup::Light::Light(LightGroup& owner, const ZenLoad::zCVobData& vob)
  :light(&owner) {
  LightSource l;
  l.setPosition(Vec3(vob.position.x,vob.position.y,vob.position.z));

  if(vob.zCVobLight.dynamic.rangeAniScale.size()>0) {
    l.setRange(vob.zCVobLight.dynamic.rangeAniScale,vob.zCVobLight.range,vob.zCVobLight.dynamic.rangeAniFPS,vob.zCVobLight.dynamic.rangeAniSmooth);
    } else {
    l.setRange(vob.zCVobLight.range);
    }

  if(vob.zCVobLight.dynamic.colorAniList.size()>0) {
    l.setColor(vob.zCVobLight.dynamic.colorAniList,vob.zCVobLight.dynamic.colorAniListFPS,vob.zCVobLight.dynamic.colorAniSmooth);
    } else {
    l.setColor(vob.zCVobLight.color);
    }

  std::lock_guard<std::recursive_mutex> guard(owner.sync);
  owner.fullGpuUpdate = true;
  owner.clearIndex();

  id = owner.light.size();
  if(owner.freeList.size()>0) {
    id = owner.freeList.back();
    owner.light[id] = std::move(l);
    } else {
    owner.light.push_back(std::move(l));
    }

  if(owner.light[id].isDynamic())
    owner.dynamicState.push_back(id);
  }

LightGroup::Light::Light(LightGroup& owner)
  :light(&owner) {
  std::lock_guard<std::recursive_mutex> guard(owner.sync);
  owner.fullGpuUpdate = true;
  owner.clearIndex();

  id = owner.light.size();
  if(owner.freeList.size()>0) {
    id = owner.freeList.back();
    owner.light[id] = LightSource();
    } else {
    owner.light.emplace_back(LightSource());
    }
  owner.dynamicState.push_back(id);
  }

LightGroup::Light::Light(World& owner, const ZenLoad::zCVobData& vob)
  :Light(owner.view()->sGlobal.lights,vob) {
  owner.view()->needToUpdateUbo = true;
  }

LightGroup::Light::Light(World& owner)
  :Light(owner.view()->sGlobal.lights) {
  owner.view()->needToUpdateUbo = true;
  }

LightGroup::Light& LightGroup::Light::operator =(LightGroup::Light&& other) {
  std::swap(light,other.light);
  std::swap(id,other.id);
  return *this;
  }

LightGroup::Light::~Light() {
  if(light!=nullptr)
    light->free(id);
  }

void LightGroup::Light::setPosition(float x, float y, float z) {
  setPosition(Vec3(x,y,z));
  }

void LightGroup::Light::setPosition(const Vec3& p) {
  if(light==nullptr)
    return;
  light->light[id].setPosition(p);
  }

void LightGroup::Light::setRange(float r) {
  if(light==nullptr)
    return;
  light->light[id].setRange(r);
  }

void LightGroup::Light::setColor(const Vec3& c) {
  if(light==nullptr)
    return;
  light->light[id].setColor(c);
  }

LightGroup::LightGroup(const SceneGlobals& scene)
  :scene(scene) {
  for(auto& u:uboBuf)
    u = scene.storage.device.ubo<Ubo>(nullptr,1);
  }

void LightGroup::dbgLights(DbgPainter& p) const {
  int cnt = 0;
  p.setBrush(Color(1,0,0,0.01f));
  //p.setBrush(Color(1,0,0,1.f));

  for(auto& i:light) {
    float r  = i.range();
    auto  pt = i.position();
    Vec3 px[9] = {};
    px[0] = pt+Vec3(-r,-r,-r);
    px[1] = pt+Vec3( r,-r,-r);
    px[2] = pt+Vec3( r, r,-r);
    px[3] = pt+Vec3(-r, r,-r);
    px[4] = pt+Vec3(-r,-r, r);
    px[5] = pt+Vec3( r,-r, r);
    px[6] = pt+Vec3( r, r, r);
    px[7] = pt+Vec3(-r, r, r);
    px[8] = pt;

    for(auto& i:px) {
      p.mvp.project(i.x,i.y,i.z);
      i.x = (i.x+1.f)*0.5f;
      i.y = (i.y+1.f)*0.5f;
      }

    int x = int(px[8].x*float(p.w));
    int y = int(px[8].y*float(p.h));

    int x0 = x, x1 = x;
    int y0 = y, y1 = y;
    float z0=px[8].z, z1=px[8].z;

    for(auto& i:px) {
      int x = int(i.x*float(p.w));
      int y = int(i.y*float(p.h));
      x0 = std::min(x0, x);
      y0 = std::min(y0, y);
      x1 = std::max(x1, x);
      y1 = std::max(y1, y);
      z0 = std::min(z0, i.z);
      z1 = std::max(z1, i.z);
      }

    if(z1<0.f || z0>1.f)
      continue;
    if(x1<0 || x0>int(p.w))
      continue;
    if(y1<0 || y0>int(p.h))
      continue;

    cnt++;
    p.painter.drawRect(x0,y0,x1-x0,y1-y0);
    p.painter.drawRect(x0,y0,3,3);
    }

  char  buf[250]={};
  std::snprintf(buf,sizeof(buf),"light count = %d",cnt);
  p.drawText(10,50,buf);
  }

void LightGroup::free(size_t id) {
  std::lock_guard<std::recursive_mutex> guard(sync);
  fullGpuUpdate = true;
  clearIndex();

  for(size_t i=0; i<dynamicState.size(); ++i)
    if(dynamicState[i]==id) {
      dynamicState[i] = dynamicState.back();
      dynamicState.pop_back();
      break;
      }
  light[id] = LightSource();
  freeList.push_back(id);
  }

size_t LightGroup::get(const Bounds& area, const LightSource** out, size_t maxOut) const {
  if(index.count!=light.size())
    mkIndex();
  return implGet(index,area,out,maxOut);
  }

void LightGroup::tick(uint64_t time) {
  for(auto i:dynamicState)
    light[i].update(time);
  }

void LightGroup::preFrameUpdate(uint8_t fId) {
  buildVbo(fId);

  Ubo ubo;
  ubo.mvp    = scene.viewProject();
  ubo.mvpInv = ubo.mvp;
  ubo.mvpInv.inverse();
  ubo.fr.make(ubo.mvp);
  uboBuf[fId].update(&ubo,0,1);
  }

void LightGroup::draw(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  auto& p = scene.storage.pLights;
  cmd.setUniforms(p,ubo[fId]);
  for(auto& i:chunks) {
    size_t sz = (i.vboGpu[fId].size()/8)*36;
    cmd.draw(i.vboGpu[fId],iboGpu,0,sz);
    }
  }

void LightGroup::setupUbo() {
  auto& device = scene.storage.device;
  auto& p      = scene.storage.pLights;

  for(int i=0;i<Resources::MaxFramesInFlight;++i) {
    auto& u = ubo[i];
    u = device.uniforms(p.layout());
    u.set(0,*scene.gbufDiffuse,Sampler2d::nearest());
    u.set(1,*scene.gbufNormals,Sampler2d::nearest());
    u.set(2,*scene.gbufDepth,  Sampler2d::nearest());
    u.set(3,uboBuf[i]);
    }
  }

size_t LightGroup::implGet(const LightGroup::Bvh& index, const Bounds& area, const LightSource** out, size_t maxOut) const {
  const Bvh* cur = &index;
  if(maxOut==0)
    return 0;
  while(true) {
    if(cur->next[0]==nullptr && cur->next[1]==nullptr) {
      size_t cnt = std::min(cur->count,maxOut);
      for(size_t i=0; i<cnt; ++i) {
        out[i] = cur->b[i];
        }
      return cnt;
      }

    bool l = cur->next[0]!=nullptr && isIntersected(cur->next[0]->bbox,area);
    bool r = cur->next[1]!=nullptr && isIntersected(cur->next[1]->bbox,area);
    if(l && r) {
      size_t cnt0 = implGet(*cur->next[0],area,out,     maxOut);
      size_t cnt1 = implGet(*cur->next[1],area,out+cnt0,maxOut-cnt0);
      return cnt0+cnt1;
      }
    else if(l) {
      cur = cur->next[0].get();
      }
    else if(r) {
      cur = cur->next[1].get();
      }
    else {
      return 0;
      }
    }
  }

void LightGroup::mkIndex() const {
  indexPtr.resize(light.size());
  for(size_t i=0; i<indexPtr.size(); ++i)
    indexPtr[i] = &light[i];
  mkIndex(index,indexPtr.data(),indexPtr.size(),0);
  }

void LightGroup::mkIndex(Bvh& id, const LightSource** b, size_t count, int depth) const {
  id.b     = b;
  id.count = count;

  if(count==1) {
    id.bbox.assign((**b).position(),(**b).range());
    return;
    }

  depth%=3;
  if(depth==0) {
    std::sort(b,b+count,[](const LightSource* a,const LightSource* b){
      return a->position().x<b->position().x;
      });
    }
  else if(depth==1) {
    std::sort(b,b+count,[](const LightSource* a,const LightSource* b){
      return a->position().y<b->position().y;
      });
    }
  else {
    std::sort(b,b+count,[](const LightSource* a,const LightSource* b){
      return a->position().z<b->position().z;
      });
    }

  size_t half = count/2;
  if(half>0) {
    if(id.next[0]==nullptr)
      id.next[0].reset(new Bvh());
    mkIndex(*id.next[0],b,half,depth+1);
    } else {
    id.next[0].reset();
    }
  if(count-half>0) {
    if(id.next[1]==nullptr)
      id.next[1].reset(new Bvh());
    mkIndex(*id.next[1],b+half,count-half,depth+1);
    } else {
    id.next[1].reset();
    }

  if(id.next[0]!=nullptr && id.next[1]!=nullptr)
    id.bbox.assign(id.next[0]->bbox,id.next[1]->bbox);
  else if(id.next[0]!=nullptr)
    id.bbox = id.next[0]->bbox;
  else if(id.next[1]!=nullptr)
    id.bbox = id.next[1]->bbox;
  }

void LightGroup::clearIndex() {
  index.next[0].reset();
  index.next[1].reset();
  index.count = 0;
  }

bool LightGroup::isIntersected(const Bounds& a, const Bounds& b) {
  if(a.bboxTr[1].x<b.bboxTr[0].x)
    return false;
  if(a.bboxTr[0].x>b.bboxTr[1].x)
    return false;
  if(a.bboxTr[1].y<b.bboxTr[0].y)
    return false;
  if(a.bboxTr[0].y>b.bboxTr[1].y)
    return false;
  if(a.bboxTr[1].z<b.bboxTr[0].z)
    return false;
  if(a.bboxTr[0].z>b.bboxTr[1].z)
    return false;
  return true;
  }

void LightGroup::buildVbo(uint8_t fId) {
  static const uint16_t ibo[36] = {
    0, 1, 3, 3, 1, 2,
    1, 5, 2, 2, 5, 6,
    5, 4, 6, 6, 4, 7,
    4, 0, 7, 7, 0, 3,
    3, 2, 7, 7, 2, 6,
    4, 5, 0, 0, 5, 1
    };

  auto& device = scene.storage.device;
  if(iboGpu.size()==0) {
    std::vector<uint16_t> iboCpu;
    iboCpu.resize(CHUNK_SIZE*36);
    for(uint16_t i=0; i<CHUNK_SIZE; ++i) {
      for(uint16_t r=0; r<36;++r) {
        iboCpu[i*36u+r] = uint16_t(i*8u+ibo[r]);
        }
      }
    iboGpu = device.ibo(iboCpu);
    }

  if(fullGpuUpdate) {
    chunks.resize((light.size()+CHUNK_SIZE-1)/CHUNK_SIZE);
    fullGpuUpdate = false;
    vboCpu.resize(light.size()*8);
    for(size_t i=0; i<light.size(); ++i)
      buildVbo(&vboCpu[i*8],light[i]);
    } else {
    for(auto i:dynamicState) {
      auto&  l   = light[i];
      buildVbo(&vboCpu[i*8],l);
      }
    }

  for(size_t i=0; i<chunks.size(); ++i) {
    auto&  ch = chunks[i];
    size_t i0  = i*CHUNK_SIZE*8;
    size_t len = std::min<size_t>(vboCpu.size()-i0,CHUNK_SIZE*8);
    if(ch.vboGpu[fId].size()!=len)
      ch.vboGpu[fId] = scene.storage.device.vboDyn(&vboCpu[i0],len); else
      ch.vboGpu[fId].update(&vboCpu[i0],0,len);
    }
  }

void LightGroup::buildVbo(LightGroup::Vertex* vbo, const LightSource& l) {
  static const Vec3 v[8] = {
    {-1,-1,-1},
    { 1,-1,-1},
    { 1, 1,-1},
    {-1, 1,-1},

    {-1,-1, 1},
    { 1,-1, 1},
    { 1, 1, 1},
    {-1, 1, 1},
    };

  auto   R   = l.currentRange();
  auto&  at  = l.position();
  auto&  cl  = l.currentColor();
  for(int r=0; r<8; ++r) {
    Vertex& vx = vbo[r];
    vx.pos   = at + v[r]*R;
    vx.cen   = Vec4(at.x,at.y,at.z,R);
    vx.color = cl;
    }
  }

