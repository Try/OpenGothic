#include "dataworker.h"

#include <Tempest/Log>
#include <Tempest/Fence>

#include "project/projectitem.h"
#include "graphics/shaders.h"
#include "utils/fileext.h"
#include "utils/workers.h"
#include "resources.h"

using namespace Tempest;

static DataWorker* instance = nullptr;

DataWorker::DataWorker() {
  instance = this;

  Tempest::Matrix4x4 p, mv, shMv[Resources::ShadowLayers];
  p.identity();
  mv.identity();
  mv.scale(0.8f,1.f,1.f);
  render.scene.setViewProject(mv,p,0,1,shMv);

  th = std::thread([this](){
    Workers::setThreadName("Data thread");
    exec();
    });
  }

DataWorker::~DataWorker() {
  {
    std::lock_guard<std::mutex> lock(sync);
    isExit = true;
  }
  workWait.notify_all();
  th.join();
  instance = nullptr;
  }

DataWorker& DataWorker::inst() {
  return *instance;
  }

void DataWorker::load(const ProjectItem& it) {
  instance->pushItem(it);
  }

void DataWorker::exec() {
  while(true) {
    ProjectItem it;
    if(!popItem(it))
      return;

    //TODO: loader by full name?! Doesn't seem to apply to how gothic game is structured..
    // auto name = std::string(it.path());
    auto name = std::string(it.name());
    FileExt::exchangeExt(name,"MRM","3DS");
    if(auto proto = Resources::loadMesh(name)) {
      auto mesh = MeshObjects::Mesh(render.itmGroup,*proto,0,0,0,false);
      mesh.setObjMatrix(Matrix4x4::mkIdentity());

      auto ret = createPreview(it, mesh, proto->bbox());
      commit(it, [&](){
        it.setPreview(ret);
        // Log::d(it.name()," - loaded");
        });
      } else {
      Log::d(it.name()," - unable to load mesh");
      }
    }
  }

void DataWorker::pushItem(const ProjectItem& it) {
  {
    std::lock_guard<std::mutex> lock(sync);
    if (isExit)
        return;
    for(auto& i:items)
      if(i.data==it.data)
        return;
    items.push_back(std::move(it));
  }
  workWait.notify_all();
  }

bool DataWorker::popItem(ProjectItem& out) {
  std::unique_lock<std::mutex> lock(sync);
  workWait.wait(lock, [this] { return isExit || !items.empty(); });
  if(isExit)
    return false;

  out = std::move(items.back());
  items.pop_back();
  return true;
  }

void DataWorker::commit(ProjectItem& out, std::function<void()> func) {
  std::unique_lock<std::mutex> lock(sync);
  items.erase(std::remove_if(items.begin(), items.end(), [&](const ProjectItem& it) { return it.data == out.data; }), items.end());
  func();
  }

auto DataWorker::createPreview(ProjectItem& itm, const MeshObjects::Mesh& mesh, const Vec3* bbox) -> std::shared_ptr<Tempest::Texture2d> {
  Tempest::Matrix4x4 viewMat;
  // inventory-like view
  {
  float    sz  = (bbox[1]-bbox[0]).length();
  auto     mv  = (bbox[1]+bbox[0])*0.5f;
  //mv = Vec3(mv.x,mv.y,mv.z);
  sz = 2.f/sz;
  if(sz>0.1f)
    sz=0.1f;

  static const float invX = 180;
  static const float invY = 0;
  static const float invZ = 45;
  Tempest::Matrix4x4 mat;
  mat.identity();
  mat.rotateOX(invX);
  mat.rotateOZ(invZ);
  mat.rotateOY(invY);
  for(int i=0; i<3; ++i){
    auto trX = mat.at(i,0);
    auto trY = mat.at(i,2);
    mat.set(i,0,trY);
    mat.set(i,2,trX);
    }
  mat.scale(sz);
  mat.translate(-mv);

  viewMat = mat;
  }

  auto& pso    = Shaders::inst().inventory;
  auto& device = Resources::device();
  auto  cmd    = device.commandBuffer();
  auto  img    = device.attachment(TextureFormat::RGBA8, 128, 128, false);
  {
    auto enc = cmd.startEncoding(device);
    enc.setFramebuffer({{img, Vec4(0), Tempest::Preserve}}); //TODO: zbuffer

    Tempest::Matrix4x4 mv = Tempest::Matrix4x4::mkIdentity();
    mv.translate(0, 0, 0.5f);
    mv.scale(0.8f,1.f,1.f);

    for(size_t i=0; i<mesh.nodesCount(); ++i) {
      auto  n = mesh.node(i);
      auto& m = n.material();

      if(auto s = n.mesh()) {
        auto sl = n.meshSlice();
        auto p  = mv;
        p.scale(1, 1, 0.25f);

        p.mul(n.position());
        p.mul(viewMat);

        enc.setBinding(0, *m.tex);
        enc.setPushData(p);
        enc.setPipeline(pso);
        enc.draw(s->vbo, s->ibo, sl.first, sl.second);
        }
      }
  }
  device.submit(cmd).wait();
  return std::make_shared<Texture2d>(std::move(textureCast<Texture2d&>(img)));
  }
