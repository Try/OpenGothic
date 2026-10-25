#include <Tempest/Application>
#include <Tempest/Event>
#include <Tempest/Gamepad>
#include <Tempest/SystemApi>
#include <Tempest/Timer>
#include <Tempest/Window>

#include <charconv>
#include <cstdio>
#include <exception>
#include <map>
#include <string_view>

namespace {
using Pad = Tempest::Gamepad;

constexpr const char* buttons[] = {
  "south", "east", "west", "north", "dpad.up", "dpad.down", "dpad.left", "dpad.right",
  "leftShoulder", "rightShoulder", "leftThumb", "rightThumb", "leftTrigger", "rightTrigger",
  "menu", "view", "home"
  };
static_assert(std::size(buttons)==size_t(Pad::Button::Count));

constexpr const char* types[] = {"connected", "disconnected", "down", "up", "reset"};
constexpr const char* reasons[] = {"none", "focus-lost", "disconnected", "overflow"};

class Probe final : public Tempest::Window {
  public:
    explicit Probe(unsigned seconds) : duration(uint64_t(seconds)*1000) {
      setWindowTitle("Tempest Gamepad — click here and operate the controller");
      timer.timeout.bind(this, &Probe::poll);
      timer.start(16);
      std::printf("probe-start seconds=%u source=Tempest::Gamepad\n", seconds);
      }

    void poll() {
      const auto elapsed = Tempest::Application::tickCount()-start;
      Pad::Event event;
      while(Pad::pollEvent(event)) {
        std::printf("t=%llu device=%llu event=%s", ull(elapsed), ull(event.device), types[size_t(event.type)]);
        if(event.type==Pad::Event::Type::ButtonDown || event.type==Pad::Event::Type::ButtonUp)
          std::printf(" button=%s", buttons[size_t(event.button)]);
        if(event.type==Pad::Event::Type::Reset)
          std::printf(" reason=%s", reasons[size_t(event.reason)]);
        std::printf("\n");
        }

      auto devices = Pad::devices();
      for(const auto& device:devices) {
        Pad::State state;
        if(!Pad::state(device.id,state))
          continue;
        const auto previous = states.find(device.id);
        if(previous==states.end()) {
          std::printf("t=%llu device=%llu name=%s buttons-mask=%x axes-mask=%x\n",
                      ull(elapsed), ull(device.id), device.name.c_str(), device.supportedButtons, device.supportedAxes);
          }
        if(previous==states.end() || previous->second.buttons!=state.buttons || previous->second.axes!=state.axes) {
          std::printf("t=%llu device=%llu state buttons=%x left=(%.4f,%.4f) right=(%.4f,%.4f) triggers=(%.4f,%.4f)\n",
                      ull(elapsed), ull(device.id), state.buttons,
                      double(state.axes[0]), double(state.axes[1]), double(state.axes[2]),
                      double(state.axes[3]), double(state.axes[4]), double(state.axes[5]));
          states[device.id] = state;
          }
        }
      for(auto it=states.begin();it!=states.end();) {
        Pad::State state;
        if(!Pad::state(it->first,state))
          it = states.erase(it);
        else
          ++it;
        }

      char title[256];
      std::snprintf(title, sizeof(title), "Tempest Gamepad — %s — devices: %zu — %llus (see Terminal log)",
                    focused ? "ACTIVE" : "INACTIVE", devices.size(), ull(elapsed/1000));
      if(elapsed/1000!=lastTitleSecond) {
        setWindowTitle(title);
        lastTitleSecond = elapsed/1000;
        }
      if(elapsed>=duration) {
        std::printf("probe-stop reason=timeout\n");
        Tempest::SystemApi::exit();
        }
      }

  private:
    static unsigned long long ull(uint64_t value) { return static_cast<unsigned long long>(value); }

    void focusEvent(Tempest::FocusEvent& event) override {
      focused = event.in;
      std::printf("t=%llu focus=%s\n", ull(Tempest::Application::tickCount()-start), focused ? "active" : "inactive");
      }

    void closeEvent(Tempest::CloseEvent& event) override {
      event.accept();
      std::printf("probe-stop reason=close\n");
      Tempest::SystemApi::exit();
      }

    Tempest::Timer timer;
    uint64_t start = Tempest::Application::tickCount();
    uint64_t duration;
    uint64_t lastTitleSecond = uint64_t(-1);
    bool focused = true;
    std::map<Pad::DeviceId, Pad::State> states;
  };
}

int main(int argc, char** argv) {
  unsigned seconds = 180;
  bool polling = false;
  for(int i=1;i<argc;++i) {
    const std::string_view arg(argv[i]);
    if(arg=="--poll") {
      polling = true;
      } else if(arg=="--seconds" && i+1<argc) {
      const std::string_view value(argv[++i]);
      const auto parsed = std::from_chars(value.data(),value.data()+value.size(),seconds);
      if(parsed.ec!=std::errc() || parsed.ptr!=value.data()+value.size() || seconds==0 || seconds>3600) {
        std::fprintf(stderr,"--seconds must be an integer in [1,3600]\n");
        return 2;
        }
      } else {
      std::fprintf(stderr,"Usage: TempestGamepadProbe [--seconds 1..3600] [--poll]\n");
      return 2;
      }
    }

  std::setvbuf(stdout,nullptr,_IOLBF,0);
  try {
    Tempest::Application app;
    Probe probe(seconds);
    if(!polling)
      return app.exec();
    while(Tempest::Application::isRunning()) {
      Tempest::Application::processEvents();
      probe.poll();
      Tempest::Application::sleep(2);
      }
    return 0;
    }
  catch(const std::exception& error) {
    std::fprintf(stderr,"probe-error: %s\n",error.what());
    return 1;
    }
  }
