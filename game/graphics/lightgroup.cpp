#include "lightgroup.h"

#include <Tempest/Dir>
#include <Tempest/Log>

#include <zenkit/Archive.hh>

#include "graphics/shaders.h"
#include "graphics/sceneglobals.h"
#include "utils/string_frm.h"
#include "world/world.h"
#include "utils/dbgpainter.h"
#include "gothic.h"

using namespace Tempest;

LightGroup::Light::Light(LightGroup::Light&& oth):owner(oth.owner), id(oth.id) {
  oth.owner = nullptr;
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
  auto& data = owner->lightSourceDesc[id];
  data.setPosition(p);

  auto& ssbo = owner->lightSourceData[id];
  ssbo.pos = p;
  owner->markAsDurty(id);
  }

void LightGroup::Light::setRange(float r) {
  if(owner==nullptr)
    return;
  auto& data = owner->lightSourceDesc[id];
  data.setRange(r);

  auto& ssbo = owner->lightSourceData[id];
  ssbo.range = r;
  owner->markAsDurty(id);
  }

void LightGroup::Light::setColor(const Vec3& c) {
  if(owner==nullptr)
    return;
  auto& data = owner->lightSourceDesc[id];
  data.setColor(c);

  auto& ssbo = owner->lightSourceData[id];
  ssbo.color = c;
  owner->markAsDurty(id);
  }

void LightGroup::Light::setColor(const std::vector<Vec3>& c, float fps, bool smooth) {
  if(owner==nullptr)
    return;
  auto& data = owner->lightSourceDesc[id];
  data.setColor(c,fps,smooth);

  auto& ssbo = owner->lightSourceData[id];
  ssbo.color = data.currentColor();
  owner->markAsDurty(id);
  }

void LightGroup::Light::setTimeOffset(uint64_t t) {
  if(owner==nullptr)
    return;
  auto& data = owner->lightSourceDesc[id];
  data.setTimeOffset(t);
  }

uint64_t LightGroup::Light::effectPrefferedTime() const {
  if(owner==nullptr)
    return 0;
  auto& data = owner->lightSourceDesc[id];
  return data.effectPrefferedTime();
  }

LightGroup::LightGroup(const SceneGlobals& scene)
  :scene(scene) {
  auto& device = Resources::device();
  for(int i=0; i<Resources::MaxFramesInFlight; ++i) {
    descPatch[i] = device.descriptors(Shaders::inst().patch);
    }

  static const uint16_t index[] = {
      0, 1, 2, 0, 2, 3,
      4, 6, 5, 4, 7, 6,
      1, 5, 2, 2, 5, 6,
      4, 0, 7, 7, 0, 3,
      3, 2, 7, 7, 2, 6,
      4, 5, 0, 0, 5, 1
    };
  ibo = device.ibo(index, sizeof(index)/sizeof(index[0]));

  try {
    auto filename = Gothic::nestedPath({u"_work", u"Data", u"Presets", u"LIGHTPRESETS.ZEN"}, Dir::FT_File);
    auto buf = zenkit::Read::from(filename);
    auto zen = zenkit::ReadArchive::from(buf.get());

    zenkit::ArchiveObject obj {};
    auto count = zen->read_int();
    for(int i = 0; i < count; ++i) {
      zen->read_object_begin(obj);

      zenkit::LightPreset preset {};
      preset.load(*zen, Gothic::inst().version().game == 1 ? zenkit::GameVersion::GOTHIC_1
                                                           : zenkit::GameVersion::GOTHIC_2);
      presets.emplace_back(std::move(preset));

      if(!zen->read_object_end()) {
        zen->skip_object(true);
        }
      }
    }
  catch(...) {
    Log::e("unable to load Zen-file: \"LIGHTPRESETS.ZEN\"");
    }
  }

LightGroup::Light LightGroup::add(const zenkit::LightPreset& vob) {
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

  std::lock_guard<std::mutex> guard(sync);
  size_t id = alloc(l.isDynamic());
  auto   lx = Light(*this, id);

  auto& ssbo = lightSourceData[lx.id];
  ssbo.pos   = l.position();
  ssbo.range = l.range();
  ssbo.color = l.color();

  auto& data = lightSourceDesc[lx.id];
  data = std::move(l);

  markAsDurtyNoSync(lx.id);
  return lx;
  }

LightGroup::Light LightGroup::add(const zenkit::VLight& vob) {
  auto l = add(static_cast<const zenkit::LightPreset&>(vob));
  l.setPosition(Vec3(vob.position.x,vob.position.y,vob.position.z));
  return l;
  }

LightGroup::Light LightGroup::add(std::string_view preset) {
  return add(findPreset(preset));
  }

void LightGroup::dbgLights(DbgPainter& p) const {
  int cnt = 0;
  //p.setBrush(Color(1,0,0,0.01f));
  p.setBrush(Color(1,0,0,1.f));

  for(auto& i:lightSourceDesc) {
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

  string_frm name("light count = ",cnt);
  p.drawText(10,50,name);
  }

size_t LightGroup::alloc(bool dynamic) {
  if(freeList.size()>0) {
    auto ret = freeList.back();
    freeList.pop_back();
    if(dynamic)
      animatedLights.insert(ret);
    markAsDurtyNoSync(ret);
    return ret;
    }
  lightSourceData.emplace_back();
  lightSourceDesc.emplace_back();
  duryBit.resize((lightSourceData.size()+32u-1u)/32u);

  auto ret = lightSourceData.size()-1;
  if(dynamic)
    animatedLights.insert(ret);
  markAsDurtyNoSync(ret);
  return ret;
  }

void LightGroup::free(size_t id) {
  std::lock_guard<std::mutex> guard(sync);
  markAsDurtyNoSync(id);
  animatedLights.erase(id);
  if(id+1==lightSourceData.size()) {
    lightSourceData.pop_back();
    lightSourceDesc.pop_back();
    duryBit.resize((lightSourceData.size()+32u-1u)/32u);
    } else {
    lightSourceDesc[id].setRange(0);
    lightSourceData[id] = LightSsbo();
    freeList.push_back(id);
    }
  }

void LightGroup::markAsDurty(size_t id) {
  std::lock_guard<std::mutex> guard(sync);
  markAsDurtyNoSync(id);
  }

void LightGroup::markAsDurtyNoSync(size_t id) {
  duryBit[id/32] |= (1u << (id%32));
  }

void LightGroup::resetDurty() {
  std::memset(duryBit.data(), 0, duryBit.size()*sizeof(duryBit[0]));
  }

RenderPipeline& LightGroup::shader() const {
  if(Gothic::options().doRayQuery)
    return Shaders::inst().lightsRq;
  return Shaders::inst().lights;
  }

const zenkit::LightPreset& LightGroup::findPreset(std::string_view preset) const {
  for(auto& i:presets) {
    if(i.preset!=preset)
      continue;
    return i;
    }
  Log::e("unknown light preset: \"",preset,"\"");
  static zenkit::LightPreset zero {};
  return zero;
  }

void LightGroup::tick(uint64_t time) {
  for(size_t i : animatedLights) {
    auto& light = lightSourceDesc[i];
    light.update(time);

    LightSsbo ssbo;
    ssbo.pos   = light.position();
    ssbo.color = light.currentColor();
    ssbo.range = light.currentRange();
    auto& dst = lightSourceData[i];
    if(std::memcmp(&dst, &ssbo, sizeof(ssbo))==0)
      continue;
    dst = ssbo;
    markAsDurtyNoSync(i);
    }
  }

void LightGroup::prepareGlobals(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  std::vector<Path>      patchBlock;
  std::vector<LightSsbo> patchData;

  if(lightSourceSsbo.byteSize()<lightSourceData.size()*sizeof(LightSsbo)) {
    auto& device  = Resources::device();
    Resources::recycle(std::move(lightSourceSsbo));
    lightSourceSsbo = device.ssbo(lightSourceData);
    resetDurty();
    allocDescriptorSet();
    return;
    }

  for(size_t i=0; i<lightSourceDesc.size(); ++i) {
    if(i%32==0 && duryBit[i/32]==0) {
      i+=31;
      continue;
      }
    if((duryBit[i/32] & (1u<<i%32))==0)
      continue;

    patchData.push_back(lightSourceData[i]);

    Path p;
    p.dst  = uint32_t(i);
    p.src  = uint32_t(patchData.size()-1);
    p.size = 1;
    if(patchBlock.size()>0) {
      auto& b = patchBlock.back();
      const uint32_t maxBlockSize = 16;
      if(b.dst+b.size==p.dst && b.size<maxBlockSize) {
        b.size++;
        continue;
        }
      }
    patchBlock.push_back(p);
    }

  if(patchBlock.empty())
    return;
  resetDurty();

  const size_t headerSize = patchBlock.size()*sizeof(Path);
  const size_t dataSize   = patchData .size()*sizeof(LightSsbo);
  for(auto& i:patchBlock) {
    i.dst  *= uint32_t(sizeof(LightSsbo));
    i.src  *= uint32_t(sizeof(LightSsbo));
    i.size *= uint32_t(sizeof(LightSsbo));

    i.src  += uint32_t(headerSize);

    // uint's in shader
    i.dst  /= sizeof(uint32_t);
    i.src  /= sizeof(uint32_t);
    i.size /= sizeof(uint32_t);
    }

  auto& device  = Resources::device();
  auto& patch   = patchSsbo[fId];
  if(patch.byteSize()<headerSize+dataSize) {
    Resources::recycle(std::move(patch));
    patch = device.ssbo(Tempest::BufferHeap::Upload, Tempest::Uninitialized, headerSize+dataSize);
    }
  patch.update(patchBlock.data(), 0,          headerSize);
  patch.update(patchData.data(),  headerSize, dataSize);

  auto& d = descPatch[fId];
  d.set(0, lightSourceSsbo);
  d.set(1, patch);

  cmd.setFramebuffer({});
  cmd.setUniforms(Shaders::inst().patch, d);
  cmd.dispatch(patchBlock.size());
  }

void LightGroup::draw(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  static bool light = true;
  if(!light)
    return;

  if(lightSourceSsbo.isEmpty())
    return;

  auto& p = shader();
  cmd.setUniforms(p, desc, &scene.originLwc, sizeof(scene.originLwc));
  cmd.draw(nullptr,ibo, 0,ibo.size(), 0,lightSourceData.size());
  }

void LightGroup::allocDescriptorSet() {
  if(lightSourceSsbo.isEmpty())
    return;

  Resources::recycle(std::move(desc));

  auto& device  = Resources::device();
  desc = device.descriptors(shader().layout());
  prepareUniforms();
  prepareRtUniforms();
  }

void LightGroup::prepareUniforms() {
  if(desc.isEmpty())
    return;
  desc.set(0, scene.uboGlobal[SceneGlobals::V_Main]);
  desc.set(1, *scene.gbufDiffuse, Sampler::nearest());
  desc.set(2, *scene.gbufNormals, Sampler::nearest());
  desc.set(3, *scene.zbuffer,     Sampler::nearest());
  desc.set(4, lightSourceSsbo);
  }

void LightGroup::prepareRtUniforms() {
  if(!Gothic::inst().options().doRayQuery)
    return;
  if(desc.isEmpty())
    return;
  desc.set(6,scene.rtScene.tlas);
  if(Resources::device().properties().descriptors.nonUniformIndexing) {
    desc.set(7, Sampler::bilinear());
    desc.set(8, scene.rtScene.tex);
    desc.set(9, scene.rtScene.vbo);
    desc.set(10,scene.rtScene.ibo);
    desc.set(11,scene.rtScene.rtDesc);
    }
  }
