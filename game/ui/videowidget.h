#pragma once

#include <Tempest/Widget>

#include <queue>

#include "resources.h"

class VideoWidget : public Tempest::Widget {
  public:
    VideoWidget();
    ~VideoWidget();

    void pushVideo(std::string_view filename);
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

    std::unique_ptr<Context>      ctx;
    Tempest::Texture2d            tex[Resources::MaxFramesInFlight];
    Tempest::Texture2d*           frame  = nullptr;
    bool                          active = false;
    bool                          restoreMusic = false;

    std::atomic_bool              hasPendingVideo{false};
    std::mutex                    syncVideo;
    std::queue<std::string>       pendingVideo;
  };

