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
    void paint(Tempest::Device& device, uint8_t fId);
    void paintEvent(Tempest::PaintEvent &event) override;

    void keyDownEvent(Tempest::KeyEvent&   event) override;
    void keyUpEvent  (Tempest::KeyEvent&   event) override;

  private:
    struct Input;
    struct Sound;
    struct SoundContext;
    struct Context;

    void  stopVideo();

    Gothic&                       gothic;
    std::unique_ptr<Context>      ctx;
    Tempest::Texture2d            tex;
    bool                          active = false;
    bool                          restoreMusic = false;

    std::atomic_bool              hasPendingVideo{false};
    std::mutex                    syncVideo;
    std::queue<Daedalus::ZString> pendingVideo;
  };

