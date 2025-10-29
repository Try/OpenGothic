#include "videowidget.h"

#include <Tempest/Painter>
#include <Tempest/TextCodec>
#include <Tempest/Log>
#include <Tempest/Application>
#include <Tempest/Platform>
#include <Tempest/Fence>

#include <cstddef>

#include "bink/video.h"
#include "graphics/shaders.h"
#include "gamemusic.h"
#include "gothic.h"

using namespace Tempest;

struct VideoWidget::Input : Bink::Video::Input {
  Input(zenkit::Read& fin):fin(fin) {}

  void read(void *dest, size_t count) override {
    if(fin.read(dest,count)!=count)
      throw std::runtime_error("i/o error");
    at += count;
    }
  void skip(size_t count) override {
    fin.seek(ptrdiff_t(count), zenkit::Whence::CUR);
    at += count;
    }
  void seek(size_t pos) override {
    fin.seek(ptrdiff_t(pos), zenkit::Whence::BEG);
    at = pos;
    }

  zenkit::Read&   fin;
  size_t          at = 0;
  };

struct VideoWidget::Sound : Tempest::SoundProducer {
  Sound(SoundContext& c, uint16_t sampleRate, bool isMono)
    :Tempest::SoundProducer(sampleRate, isMono ? 1 : 2), ctx(c), channels(isMono ? 1 : 2) {
    }
  void renderSound(int16_t *out, size_t n) override;

  SoundContext& ctx;
  size_t        channels = 2;
  };

struct VideoWidget::SoundContext {
  SoundContext(Context& ctx, SoundDevice& dev, uint16_t sampleRate, bool isMono):  ctx(ctx) {
    snd = dev.load(std::unique_ptr<VideoWidget::Sound>(new VideoWidget::Sound(*this,sampleRate,isMono)));
    }

  ~SoundContext() {
    snd = SoundEffect();
    }

  void play() {
    snd.play();
    }

  void pushSamples(const std::vector<float>& s) {
    std::lock_guard<std::mutex> guard(syncSamples);
    size_t sz = samples.size();
    samples.resize(sz+s.size());
    std::memcpy(samples.data()+sz, s.data(), s.size()*sizeof(s[0]));
    }

  Context&             ctx;
  Tempest::SoundEffect snd;
  std::mutex           syncSamples;
  std::vector<float>   samples;
  };

void VideoWidget::Sound::renderSound(int16_t *out, size_t n) {
  n = n*channels; // stereo

  std::lock_guard<std::mutex> guard(ctx.syncSamples);
  auto& s = ctx.samples;
  if(s.size()<n)
    return;
  for(size_t i=0; i<n; ++i) {
    float v = s[i];
    out[i] = (v < -1.00004566f ? int16_t(-32768) : (v > 1.00001514f ? int16_t(32767) : int16_t(v * 32767.5f)));
    }
  ctx.samples.erase(ctx.samples.begin(),ctx.samples.begin()+int(n));
  }

struct VideoWidget::Context {
  Context(std::unique_ptr<zenkit::Read>&& f) : fin(std::move(f)), input(*fin), vid(&input) {
    sndCtx.resize(vid.audioCount());
    for(size_t i=0; i<sndCtx.size(); ++i) {
      auto& aud = vid.audio(uint8_t(i));
      sndCtx[i].reset(new SoundContext(*this,sndDev,aud.sampleRate,aud.isMono));
      }

    const float volume = Gothic::inst().settingsGetF("SOUND","soundVolume");
    sndDev.setGlobalVolume(volume);
    for(size_t i=0; i<vid.audioCount(); ++i)
      sndCtx[i]->play();

    for(size_t i=0; i<Resources::MaxFramesInFlight; ++i) {
      cmd [i] = Resources::device().commandBuffer();
      sync[i] = Resources::device().fence();
      }

    frameTime = Application::tickCount();
    }

  ~Context() {
    for(auto& i:sync)
      i.wait();
    }

  void advance(Tempest::Device& device, uint8_t fId) {
    auto& f = vid.nextFrame();
    yuvToRgba(f, device, fId);
    for(size_t i=0; i<vid.audioCount(); ++i)
      sndCtx[i]->pushSamples(f.audio(uint8_t(i)).samples);

    const uint64_t destTick = frameTime+(1000*vid.fps().den*vid.currentFrame())/vid.fps().num;
    const uint64_t tick     = Application::tickCount();
    if(destTick > tick) {
      Application::sleep(uint32_t(destTick-tick));
      return;
      }
    }

  void yuvToRgba(const Bink::Frame& f, Tempest::Device& device, uint8_t fId) {
    if(uint32_t(frameImg.w())!=f.width() || uint32_t(frameImg.h())!=f.height()) {
      Resources::recycle(std::move(frameImg));
      frameImg = device.attachment(Tempest::RGBA8, f.width(), f.height());
      }
    sync[fId].wait();

    // alignment for ssbo offsets
    auto alignBuf = [](size_t size, size_t align) { return (size+align-1) & ~(align-1); };

    auto& planeY = f.plane(0);
    auto& planeU = f.plane(1);
    auto& planeV = f.plane(2);

    auto  align  = std::max<size_t>(device.properties().ssbo.offsetAlign, 4);
    auto  sizeY  = alignBuf(f.height()*planeY.stride(),     align);
    auto  sizeU  = alignBuf((f.height()/2)*planeU.stride(), align);
    auto  sizeV  = alignBuf((f.height()/2)*planeV.stride(), align);
    auto  size   = sizeY + sizeU + sizeV;

    auto& stage  = staging[fId];
    if(stage.byteSize()!=size) {
      Resources::recycle(std::move(stage));
      stage = device.ssbo(BufferHeap::Upload, Uninitialized, size);
      }
    stage.update(planeY.data(), 0,           (f.height())  *planeY.stride());
    stage.update(planeU.data(), sizeY,       (f.height()/2)*planeU.stride());
    stage.update(planeV.data(), sizeY+sizeU, (f.height()/2)*planeV.stride());

    {
      auto cmd = this->cmd[fId].startEncoding(device);
      cmd.setFramebuffer({{frameImg, Vec4(0), Tempest::Preserve}});

      struct Push { uint32_t strideY; uint32_t strideU; uint32_t strideV; } push = {};
      push.strideY = planeY.stride();
      push.strideU = planeU.stride();
      push.strideV = planeV.stride();

      cmd.setDebugMarker("Bink");
      cmd.setPushData(push);
      cmd.setBinding(0, stage, 0);
      cmd.setBinding(1, stage, sizeY);
      cmd.setBinding(2, stage, sizeY + sizeU);
      cmd.setPipeline(Shaders::inst().bink);
      cmd.draw(nullptr, 0, 3);
    }
    device.submit(this->cmd[fId], sync[fId]);
    }

  [[deprecated]]
  void yuvToRgba(const Bink::Frame& f, Pixmap& pm) {
    if(pm.w()!=f.width() || pm.h()!=f.height())
      pm = Pixmap(f.width(),f.height(),TextureFormat::RGBA8);
    auto& planeY = f.plane(0);
    auto& planeU = f.plane(1);
    auto& planeV = f.plane(2);
    auto  dst    = reinterpret_cast<uint8_t*>(pm.data());

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

  std::unique_ptr<zenkit::Read> fin;
  Input                         input;
  Bink::Video                   vid;
  //Pixmap                        pm;
  uint64_t                      frameTime = 0;

  Tempest::Attachment           frameImg;
  Tempest::CommandBuffer        cmd    [Resources::MaxFramesInFlight];
  Tempest::StorageBuffer        staging[Resources::MaxFramesInFlight];
  Tempest::Fence                sync   [Resources::MaxFramesInFlight];

  Tempest::SoundDevice          sndDev;
  std::vector<std::unique_ptr<SoundContext>> sndCtx;
  };

VideoWidget::VideoWidget() {
  setCursorShape(CursorShape::Hidden);
  }

VideoWidget::~VideoWidget() {
  }

void VideoWidget::pushVideo(std::string_view filename) {
  const int scaleVideos = Gothic::settingsGetI("GAME", "scaleVideos");
  if(scaleVideos<0)
    return;
  std::lock_guard<std::mutex> guard(syncVideo);
  pendingVideo.emplace(filename);
  hasPendingVideo.store(true);
  }

bool VideoWidget::isActive() const {
  return ctx!=nullptr || hasPendingVideo.load();
  }

void VideoWidget::tick() {
  if(ctx!=nullptr && ctx->isEof()) {
    stopVideo();
    }

  if(ctx!=nullptr)
    return;

  if(!hasPendingVideo)
    return;

  std::string filename;
  {
  std::lock_guard<std::mutex> guard(syncVideo);
  if(pendingVideo.empty())
    return;
  filename = std::move(pendingVideo.front());
  pendingVideo.pop();
  if(pendingVideo.size()==0)
    hasPendingVideo.store(false);
  }

  std::unique_ptr<zenkit::Read> read;
  if(auto* entry = Resources::vdfsIndex().find(filename)) {
    read = entry->open_read();
    }
  else if(auto* entry = Resources::vdfsIndex().find(filename+".bik")) {
    // some api-calls are missing extension
    read = entry->open_read();
    }
  else {
    Log::e("unable to locate video file: \"",filename,"\"");
    stopVideo();
    return;
    }

  try {
    ctx.reset(new Context(std::move(read)));
    if(!active) {
      active       = true;
      restoreMusic = GameMusic::inst().isEnabled();
      GameMusic::inst().setEnabled(false);
      }
    }
  catch(...){
    Log::e("unable to play video: \"",filename,"\"");
    stopVideo();
    }
  }

void VideoWidget::keyDownEvent(KeyEvent& event) {
  if(event.key==Event::K_ESCAPE) {
    stopVideo();
    }
  }

void VideoWidget::keyUpEvent(KeyEvent&) {
  }

void VideoWidget::mouseDownEvent(Tempest::MouseEvent& event) {
  if(!active) {
    event.ignore();
    return;
    }
#if defined(__MOBILE_PLATFORM__)
  stopVideo();
#endif
  }

void VideoWidget::stopVideo() {
  ctx.reset();
  if(!hasPendingVideo) {
    if(restoreMusic && !GameMusic::inst().isEnabled())
      GameMusic::inst().setEnabled(true);
    active = false;
    }
  update();
  }

void VideoWidget::paint(Tempest::Device& device, uint8_t fId) {
  if(ctx==nullptr)
    return;
  try {
    ctx->advance(device, fId);
    frame = &textureCast<Texture2d&>(ctx->frameImg);
    update();
    }
  catch(const Bink::VideoDecodingException& e) { // video exception is recoverable
    Log::e("video decoding error. frame: ",ctx->vid.currentFrame(),", what: \"", e.what(), "\"");
    }
  catch(...) {
    Log::e("video decoding error. frame: ",ctx->vid.currentFrame());
    ctx.reset();
    }
  }

void VideoWidget::paintEvent(PaintEvent& e) {
  if(ctx==nullptr || frame==nullptr)
    return;
  float k  = float(w())/float(frame->w());
  int   vh = int(k*float(frame->h()));

  Painter p(e);
  p.setBrush(Color(0,0,0,1));
  p.drawRect(0,0,w(),h());

  p.setBrush(Brush(*frame,Painter::NoBlend,ClampMode::ClampToEdge));
  p.drawRect(0,(h()-vh)/2,w(),vh,
             0,0,p.brush().w(),p.brush().h());
  }
