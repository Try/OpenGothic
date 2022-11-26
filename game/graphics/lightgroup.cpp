#include "lightgroup.h"

#include <Tempest/Dir>
#include <Tempest/Log>

#include "graphics/shaders.h"
#include "graphics/sceneglobals.h"
#include "utils/string_frm.h"
#include "world/world.h"
#include "utils/gthfont.h"
#include "utils/workers.h"
#include "utils/dbgpainter.h"
#include "bounds.h"
#include "gothic.h"

using namespace Tempest;

size_t LightGroup::LightBucket::alloc() {
  for(auto& i:updated)
    i = false;
  if(freeList.size()>0) {
    auto ret = freeList.back();
    freeList.pop_back();
    return ret;
    }
  data.emplace_back();
  light.emplace_back();
  return data.size()-1;
  }

void LightGroup::LightBucket::free(size_t id) {
  for(auto& i:updated)
    i = false;
  if(id+1==data.size()) {
    data.pop_back();
    light.pop_back();
    } else {
    light[id].setRange(0);
    data[id] = LightSsbo();
    freeList.push_back(id);
    }
  }


LightGroup::Light::Light(LightGroup::Light&& oth):owner(oth.owner), id(oth.id) {
  oth.owner = nullptr;
  }

LightGroup::Light::Light(LightGroup& owner)
  :owner(&owner) {
  std::lock_guard<std::recursive_mutex> guard(owner.sync);
  id = owner.alloc(true);
  }

LightGroup::Light::Light(LightGroup& owner, const phoenix::vobs::light_preset& vob)
  :owner(&owner) {
  LightSource l;
  l.setPosition(Vec3(0, 0, 0));

  if(!vob.range_animation_scale.empty()) {
    l.setRange(vob.range_animation_scale,vob.range,vob.range_animation_fps,vob.range_animation_smooth);
  } else {
    l.setRange(vob.range);
  }

  if(!vob.color_animation_list.empty()) {
    l.setColor(vob.color_animation_list,vob.color_animation_fps,vob.color_animation_smooth);
    } else {
    l.setColor(Vec3(vob.color.r / 255.f, vob.color.g / 255.f, vob.color.b / 255.f));
    }

  std::lock_guard<std::recursive_mutex> guard(owner.sync);
  id = owner.alloc(l.isDynamic());
  auto& ssbo = owner.get(id);
  ssbo.pos   = l.position();
  ssbo.range = l.range();
  ssbo.color = l.color();

  auto& data = owner.getL(id);
  data = std::move(l);
  }

LightGroup::Light::Light(LightGroup& owner, const phoenix::vobs::light& vob)
      :owner(&owner) {
  LightSource l;
  l.setPosition(Vec3(vob.position.x,vob.position.y,vob.position.z));

  if(!vob.range_animation_scale.empty()) {
    l.setRange(vob.range_animation_scale,vob.range,vob.range_animation_fps,vob.range_animation_smooth);
  } else {
    l.setRange(vob.range);
  }

  if(!vob.color_animation_list.empty()) {
    l.setColor(vob.color_animation_list,vob.color_animation_fps,vob.color_animation_smooth);
  } else {
    l.setColor(Vec3(vob.color.r / 255.f, vob.color.g / 255.f, vob.color.b / 255.f));
  }

  std::lock_guard<std::recursive_mutex> guard(owner.sync);
  id = owner.alloc(l.isDynamic());
  auto& ssbo = owner.get(id);
  ssbo.pos   = l.position();
  ssbo.range = l.range();
  ssbo.color = l.color();

  auto& data = owner.getL(id);
  data = std::move(l);
}

LightGroup::Light::Light(World& owner, std::string_view preset)
  :Light(owner,owner.view()->sGlobal.lights.findPreset(preset)){
  setTimeOffset(owner.tickCount());
  }

LightGroup::Light::Light(World& owner, const phoenix::vobs::light_preset& vob)
  :Light(owner.view()->sGlobal.lights,vob) {
  setTimeOffset(owner.tickCount());
  }

LightGroup::Light::Light(World& owner, const phoenix::vobs::light& vob)
  :Light(owner.view()->sGlobal.lights,vob){
  setTimeOffset(owner.tickCount());
}

LightGroup::Light::Light(World& owner)
  :Light(owner.view()->sGlobal.lights) {
  setTimeOffset(owner.tickCount());
  }

LightGroup::Light& LightGroup::Light::operator =(LightGroup::Light&& other) {
  std::swap(owner,other.owner);
  std::swap(id,other.id);
  return *this;
  }

LightGroup::Light::~Light() {
  if(owner!=nullptr)
    owner->free(id);
  }

void LightGroup::Light::setPosition(float x, float y, float z) {
  setPosition(Vec3(x,y,z));
  }

void LightGroup::Light::setPosition(const Vec3& p) {
  if(owner==nullptr)
    return;
  auto& ssbo = owner->get(id);
  ssbo.pos = p;

  auto& data = owner->getL(id);
  data.setPosition(p);
  }

void LightGroup::Light::setRange(float r) {
  if(owner==nullptr)
    return;
  auto& ssbo = owner->get(id);
  ssbo.range = r;

  auto& data = owner->getL(id);
  data.setRange(r);
  }

void LightGroup::Light::setColor(const Vec3& c) {
  if(owner==nullptr)
    return;
  auto& ssbo = owner->get(id);
  ssbo.color = c;

  auto& data = owner->getL(id);
  data.setColor(c);
  }

void LightGroup::Light::setColor(const std::vector<Vec3>& c, float fps, bool smooth) {
  if(owner==nullptr)
    return;
  auto& data = owner->getL(id);
  data.setColor(c,fps,smooth);

  auto& ssbo = owner->get(id);
  ssbo.color = data.currentColor();
  }

void LightGroup::Light::setTimeOffset(uint64_t t) {
  if(owner==nullptr)
    return;
  auto& data = owner->getL(id);
  data.setTimeOffset(t);
  }

uint64_t LightGroup::Light::effectPrefferedTime() const {
  if(owner==nullptr)
    return 0;
  auto& data = owner->getL(id);
  return data.effectPrefferedTime();
  }

LightGroup::LightGroup(const SceneGlobals& scene)
  :scene(scene) {
  auto& device = Resources::device();
  for(auto& u:uboBuf)
    u = device.ubo<Ubo>(nullptr,1);

  LightBucket* bucket[2] = {&bucketSt, &bucketDyn};
  for(auto b:bucket) {
    for(int i=0;i<Resources::MaxFramesInFlight;++i) {
      auto& u = b->ubo[i];
      u = device.descriptors(shader().layout());
      }
    }

  static const uint16_t index[36] = {
    0, 1, 3, 3, 1, 2,
    1, 5, 2, 2, 5, 6,
    5, 4, 6, 6, 4, 7,
    4, 0, 7, 7, 0, 3,
    3, 2, 7, 7, 2, 6,
    4, 5, 0, 0, 5, 1
    };
  ibo = device.ibo(index,36);

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
  vbo = device.vbo(v,8);

  try {
    auto filename = Gothic::inst().nestedPath({u"_work", u"Data", u"Presets", u"LIGHTPRESETS.ZEN"}, Dir::FT_File);
    auto buf = phoenix::buffer::mmap(filename);
    auto zen = phoenix::archive_reader::open(buf);

    phoenix::archive_object obj {};
    auto count = zen->read_int();
    for (int i = 0; i < count; ++i) {
      zen->read_object_begin(obj);

      presets.push_back(phoenix::vobs::light_preset::parse(
          *zen,
          Gothic::inst().version().game == 1 ? phoenix::game_version::gothic_1
                                             : phoenix::game_version::gothic_2));

      if (!zen->read_object_end()) {
        zen->skip_object(true);
      }
    }
  } catch(...) {
    Log::e("unable to load Zen-file: \"LIGHTPRESETS.ZEN\"");
    }
  }

void LightGroup::dbgLights(DbgPainter& p) const {
  int cnt = 0;
  //p.setBrush(Color(1,0,0,0.01f));
  p.setBrush(Color(1,0,0,1.f));

  const LightBucket* bucket[2] = {&bucketSt, &bucketDyn};
  for(auto b:bucket) {
    for(auto& i:b->light) {
      auto pt = i.position();
      float l = 10;
      p.drawLine(pt-Vec3(l,0,0),pt+Vec3(l,0,0));
      p.drawLine(pt-Vec3(0,l,0),pt+Vec3(0,l,0));
      p.drawLine(pt-Vec3(0,0,l),pt+Vec3(0,0,l));
      /*
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
      */
      }
    }

  string_frm name("light count = ",cnt);
  p.drawText(10,50,name);
  }

void LightGroup::free(size_t id) {
  std::lock_guard<std::recursive_mutex> guard(sync);
  if(id & staticMask)
    bucketSt .free(id^staticMask); else
    bucketDyn.free(id);
  }

LightGroup::LightSsbo& LightGroup::get(size_t id) {
  if(id & staticMask) {
    for(auto& i:bucketSt.updated)
      i = false;
    return bucketSt.data[id^staticMask];
    }

  for(auto& i:bucketDyn.updated)
    i = false;
  return bucketDyn.data[id];
  }

LightSource& LightGroup::getL(size_t id) {
  if(id & staticMask) {
    return bucketSt.light[id^staticMask];
    }
  return bucketDyn.light[id];
  }

RenderPipeline& LightGroup::shader() const {
  if(Gothic::inst().doRayQuery())
    return Shaders::inst().lightsRq;
  return Shaders::inst().lights;
  }

const phoenix::vobs::light_preset& LightGroup::findPreset(std::string_view preset) const {
  for(auto& i:presets) {
    if(i.preset!=preset)
      continue;
    return i;
    }
  Log::e("unknown light preset: \"",std::string(preset),"\"");
  static phoenix::vobs::light_preset zero {};
  return zero;
  }

void LightGroup::tick(uint64_t time) {
  for(size_t i=0; i<bucketDyn.light.size(); ++i) {
    auto& light = bucketDyn.light[i];
    light.update(time);

    auto& ssbo = bucketDyn.data[i];
    ssbo.pos   = light.position();
    ssbo.color = light.currentColor();
    ssbo.range = light.currentRange();

    for(auto& updated:bucketDyn.updated)
      updated = false;
    }
  }

void LightGroup::preFrameUpdate(uint8_t fId) {
  auto& device = Resources::device();
  LightBucket* bucket[2] = {&bucketSt, &bucketDyn};
  for(auto b:bucket) {
    if(b->updated[fId])
      continue;
    b->updated[fId] = true;
    if(b->ssbo[fId].byteSize()==b->data.size()*sizeof(b->data[0])) {
      b->ssbo[fId].update(b->data);
      } else {
      b->ssbo[fId] = device.ssbo(BufferHeap::Upload,b->data);
      b->ubo [fId].set(4,b->ssbo[fId]);
      }
    }

  Frustrum fr;
  fr.make(scene.viewProject(),1,1);

  Ubo ubo;
  ubo.mvp    = scene.viewProject();
  ubo.mvpInv = ubo.mvp;
  ubo.mvpInv.inverse();
  std::memcpy(ubo.fr,fr.f,sizeof(ubo.fr));

  uboBuf[fId].update(&ubo,0,1);
  }

void LightGroup::draw(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  static bool light = true;
  if(!light)
    return;
  if(Gothic::inst().doRayQuery() && scene.tlas==nullptr)
    return;

  auto& p = shader();
  if(bucketSt.data.size()>0) {
    cmd.setUniforms(p,bucketSt.ubo[fId]);
    cmd.draw(vbo,ibo, 0,ibo.size(), 0,bucketSt.data.size());
    }
  if(bucketDyn.data.size()>0) {
    cmd.setUniforms(p,bucketDyn.ubo[fId]);
    cmd.draw(vbo,ibo, 0,ibo.size(), 0,bucketDyn.data.size());
    }
  }

void LightGroup::setupUbo() {
  LightBucket* bucket[2] = {&bucketSt, &bucketDyn};
  for(auto b:bucket) {
    for(int i=0;i<Resources::MaxFramesInFlight;++i) {
      auto& u = b->ubo[i];
      u.set(0,*scene.gbufDiffuse,Sampler::nearest());
      u.set(1,*scene.gbufNormals,Sampler::nearest());
      u.set(2,*scene.gbufDepth,  Sampler::nearest());
      u.set(3,uboBuf[i]);
      if(Gothic::inst().doRayQuery() && scene.tlas!=nullptr) {
        if(Resources::device().properties().bindless.nonUniformIndexing) {
          u.set(6,scene.bindless.tex);
          u.set(7,scene.bindless.vbo);
          u.set(8,scene.bindless.ibo);
          u.set(9,scene.bindless.iboOffset);
          }
        // Workaround for https://github.com/KhronosGroup/Vulkan-ValidationLayers/pull/4219
        u.set(5,*scene.tlas);
        }
      }
    }
  }

size_t LightGroup::alloc(bool dynamic) {
  size_t ret = 0;
  if(dynamic) {
    ret = bucketDyn.alloc();
    } else {
    ret = bucketSt .alloc();
    ret |= staticMask;
    }
  return ret;
  }
