#include "videowidget.h"

#include <Tempest/Painter>
#include <Tempest/TextCodec>
#include <Tempest/Log>
#include <Tempest/Application>

#include "bink/video.h"
#include "utils/fileutil.h"
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
    if(pm.w()!=f.width() || pm.h()!=f.height())
      pm = Pixmap(f.width(),f.height(),Pixmap::Format::RGBA);

    yuvToRgba(f,pm);
    }

  void yuvToRgba(const Bink::Frame& f,Pixmap& pm) {
    auto planeY   = f.plane(0);
    auto planeU   = f.plane(1);
    auto planeV   = f.plane(2);
    auto dst = reinterpret_cast<uint8_t*>(pm.data());

    const uint32_t w = pm.w();
    for(uint32_t y=0; y<pm.h(); ++y)
      for(uint32_t x=0; x<w; ++x) {
        uint8_t* rgb = &dst[(x+y*w)*4];
        float Y = planeY.at(x,  y  );
        float U = planeU.at(x/2,y/2);
        float V = planeV.at(x/2,y/2);

        float r = 1.164f * (Y - 16.f) + 1.596f * (V - 128.f);
        float g = 1.164f * (Y - 16.f) - 0.813f * (V - 128.f) - 0.391f * (U - 128.f);
        float b = 1.164f * (Y - 16.f) + 2.018f * (U - 128.f);

        r = std::max(0.f,std::min(r,255.f));
        g = std::max(0.f,std::min(g,255.f));
        b = std::max(0.f,std::min(b,255.f));

        rgb[0] = uint8_t(r);
        rgb[1] = uint8_t(g);
        rgb[2] = uint8_t(b);
        rgb[3] = 255;
        }
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
  }

VideoWidget::~VideoWidget() {
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

  auto path  = gothic.nestedPath({u"_work",u"Data",u"Video"},Dir::FT_Dir);
  auto fname = TextCodec::toUtf16(filename.c_str());
  auto f     = FileUtil::caseInsensitiveSegment(path,fname.c_str(),Dir::FT_File);
  if(!FileUtil::exists(f)) {
    // some api-calls are missing extension
    f = FileUtil::caseInsensitiveSegment(f,u".bik",Dir::FT_File);
    }

  try {
    ctx.reset(new Context(f));
    }
  catch(...){
    Log::e("unable to play video: \"",filename.c_str(),"\"");
    }
  }

void VideoWidget::paint(Tempest::Device& device, Tempest::Encoder<CommandBuffer>& /*cmd*/, uint8_t fId) {
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

void VideoWidget::keyUpEvent(KeyEvent&) {
  }

void VideoWidget::paintEvent(PaintEvent& e) {
  if(last==nullptr)
    return;
  Painter p(e);
  p.setBrush(Brush(*last,Painter::NoBlend));
  p.drawRect(0,0,w(),h(),
             0,0,p.brush().w(),p.brush().h());
  }
