#include "videowidget.h"

#include <Tempest/Painter>
#include <Tempest/TextCodec>
#include <Tempest/Log>
#include <Tempest/Application>

#include "bink/video.h"
#include "gothic.h"

using namespace Tempest;

struct VideoWidget::Input : Bink::Video::Input {
  Input(Tempest::RFile& fin):fin(fin) {}

  void read(void *dest, size_t count) override {
    if(fin.read(dest,count)!=count)
      throw std::runtime_error("i/o error");
    at+=count;
    }
  void skip(size_t count) override {
    fin.seek(count);
    at+=count;
    }
  void seek(size_t pos) override {
    if(pos<at)
      fin.unget(at-pos); else
      fin.seek (pos-at);
    }

  Tempest::RFile& fin;
  size_t          at=0;
  };

struct VideoWidget::Context {
  Context(const std::u16string& path) : fin(path), input(fin), vid(&input) {
    frameTime = Application::tickCount();
    }

  void advance() {
    uint64_t frameDest = (Application::tickCount()-frameTime)/(1000/25); //25fps
    if(frameDest<=vid.currentFrame()) {
      return;
      }

    auto& f = vid.nextFrame();
    if(pm.w()!=f.width() || pm.h()!=f.height()) {
      pm = Pixmap(f.width(),f.height(),Pixmap::Format::R);
      }
    std::memcpy(pm.data(),f.plane(0).data(),pm.dataSize());
    }

  bool isEof() const {
    return vid.currentFrame()>=vid.frameCount();
    }

  Tempest::RFile fin;
  Input          input;
  Bink::Video    vid;
  Pixmap         pm;
  uint64_t       frameTime = 0;
  };

VideoWidget::VideoWidget(Gothic& gth)
  :gothic(gth) {
  //gothic.pushPause();
  }

VideoWidget::~VideoWidget() {
  //gothic.popPause();
  }

void VideoWidget::pushVideo(const Daedalus::ZString& filename) {
  while(true) {
    {
    std::lock_guard<std::mutex> guard(syncVideo);
    if(pendingVideo.empty()) {
      pendingVideo = filename;
      return;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

  }

bool VideoWidget::isActive() const {
  return ctx!=nullptr;
  }

void VideoWidget::tick() {
  if(ctx!=nullptr && ctx->isEof()) {
    ctx.reset();
    last = nullptr;
    update();
    }

  if(ctx!=nullptr) {
    return;
    }

  Daedalus::ZString filename;
  {
  std::lock_guard<std::mutex> guard(syncVideo);
  if(pendingVideo.empty())
    return;
  filename = std::move(pendingVideo);
  }

  auto path = gothic.nestedPath({u"_work",u"Data",u"Video"},Dir::FT_Dir);
  path.append(TextCodec::toUtf16(filename.c_str()));
  try {
    ctx.reset(new Context(path));
    }
  catch(...){
    Log::e("unable to play video: \"",filename.c_str(),"\"");
    }
  }

void VideoWidget::paint(Tempest::Device& device, Tempest::Encoder<CommandBuffer>& cmd, uint8_t fId) {
  if(ctx==nullptr)
    return;
  try {
    ctx->advance();
    tex[fId] = device.loadTexture(ctx->pm);
    last = &tex[fId];
    update();
    }
  catch(...) {
    Log::e("video decoding error. frame: \"",ctx->vid.currentFrame(),"\"");
    ctx.reset();
    last = nullptr;
    }
  }

void VideoWidget::keyDownEvent(KeyEvent& event) {
  if(event.key==Event::K_ESCAPE) {
    ctx.reset(nullptr);
    last = nullptr;
    update();
    }
  }

void VideoWidget::keyUpEvent(KeyEvent& event) {

  }

void VideoWidget::paintEvent(PaintEvent& e) {
  if(last==nullptr)
    return;
  Painter p(e);
  p.setBrush(Brush(*last,Painter::NoBlend));
  p.drawRect(0,0,w(),h(),
             0,0,p.brush().w(),p.brush().h());
  }
