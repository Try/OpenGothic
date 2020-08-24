#pragma once

#include <Tempest/Widget>
#include <daedalus/ZString.h>

#include <queue>

#include "resources.h"

class Gothic;

class VideoWidget : public Tempest::Widget {
  public:
    VideoWidget(Gothic& gth);
    ~VideoWidget();

    void pushVideo(const Daedalus::ZString& filename);
    bool isActive() const;

    void tick();
    void paint(Tempest::Device& device, Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

    void keyDownEvent(Tempest::KeyEvent&   event) override;
    void keyUpEvent  (Tempest::KeyEvent&   event) override;

  private:
    struct Input;
    struct Sound;
    struct SoundContext;
    class  Context;

    void paintEvent(Tempest::PaintEvent &event) override;

    Gothic&                   gothic;
    std::unique_ptr<Context>  ctx;

    Tempest::Texture2d        tex[Resources::MaxFramesInFlight];
    Tempest::Texture2d*       last = nullptr;

    std::mutex                    syncVideo;
    std::queue<Daedalus::ZString> pendingVideo;
  };

