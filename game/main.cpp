#include <Tempest/Window>
#include <Tempest/Application>
#include <Tempest/Log>

#include <zenkit/Logger.hh>

#include <Tempest/VulkanApi>

#if defined(_MSC_VER)
#include <Tempest/DirectX12Api>
#endif

#if defined(__APPLE__)
#include <Tempest/MetalApi>
#endif

#if defined(__IOS__)
#include "utils/installdetect.h"
#endif

#if defined(__ANDROID__)
#include <android/log.h>
#include <jni.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#endif

#include "utils/crashlog.h"
#include "mainwindow.h"
#include "gothic.h"
#include "build.h"
#include "commandline.h"

#include <dmusic.h>

#if defined(__ANDROID__)
static std::string androidGamePath;

static std::string androidPath(JNIEnv* env, jstring path) {
  if(path==nullptr)
    throw std::invalid_argument("Missing application storage path");
  const char* text = env->GetStringUTFChars(path,nullptr);
  if(text==nullptr)
    throw std::runtime_error("Unable to read application storage path");
  try {
    std::string result(text);
    env->ReleaseStringUTFChars(path,text);
    return result;
    }
  catch(...) {
    env->ReleaseStringUTFChars(path,text);
    throw;
    }
  }

static void throwAndroidError(JNIEnv* env, const char* message) {
  if(env->ExceptionCheck())
    return;
  jclass type = env->FindClass("java/lang/IllegalStateException");
  if(type!=nullptr)
    env->ThrowNew(type,message);
  }

extern "C" JNIEXPORT void JNICALL Java_org_opengothic_app_GothicActivity_prepareStorage(JNIEnv* env, jclass, jstring writablePath, jstring gamePath) {
  try {
    std::filesystem::current_path(androidPath(env,writablePath));
    androidGamePath = androidPath(env,gamePath);
    }
  catch(const std::exception& e) {
    throwAndroidError(env,e.what());
    }
  catch(...) {
    throwAndroidError(env,"Unable to initialize application storage");
    }
  }
#endif

std::string_view selectDevice(const Tempest::AbstractGraphicsApi& api) {
  auto d = api.devices();

  static Tempest::Device::Props p;
  for(auto& i:d)
    // if(i.type==Tempest::DeviceType::Integrated) {
    if(i.type==Tempest::DeviceType::Discrete) {
      p = i;
      return p.name;
      }
  if(d.size()>0) {
    p = d[0];
    return p.name;
    }
  return "";
  }

std::unique_ptr<Tempest::AbstractGraphicsApi> mkApi(const CommandLine& g) {
  Tempest::ApiFlags flg = g.isValidationMode() ? Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags;
  switch(g.graphicsApi()) {
    case CommandLine::DirectX12:
#if defined(_MSC_VER)
      return std::make_unique<Tempest::DirectX12Api>(flg);
#else
      break;
#endif
    case CommandLine::Vulkan:
#if !defined(__APPLE__)
      return std::make_unique<Tempest::VulkanApi>(flg);
#else
      break;
#endif
    }

#if defined(__APPLE__)
  return std::make_unique<Tempest::MetalApi>(flg);
#else
  return std::make_unique<Tempest::VulkanApi>(flg);
#endif
  }

int main(int argc,const char** argv) {
#if defined(__ANDROID__)
  const char* androidArgs[] = {"Gothic2Notr", "-g", androidGamePath.c_str(), "-rt", "0", "-ms", "0"};
  argc = sizeof(androidArgs)/sizeof(androidArgs[0]);
  argv = androidArgs;
#endif
#if defined(__IOS__)
  {
    auto appdir = InstallDetect::applicationSupportDirectory();
    std::filesystem::current_path(appdir);
  }
#endif

  try {
    static Tempest::WFile logFile("log.txt");
    Tempest::Log::setOutputCallback([](Tempest::Log::Mode mode, const char* text) {
#if defined(__ANDROID__)
      const int priority = mode==Tempest::Log::Error ? ANDROID_LOG_ERROR : mode==Tempest::Log::Debug ? ANDROID_LOG_DEBUG : ANDROID_LOG_INFO;
      __android_log_write(priority,"OpenGothic",text);
#endif
      logFile.write(text,std::strlen(text));
      logFile.write("\n",1);
      if(mode==Tempest::Log::Error)
        logFile.flush();
      });
    }
  catch(...) {
    Tempest::Log::e("unable to setup logfile - fallback to console log");
    }
  CrashLog::setup();

  zenkit::Logger::set(zenkit::LogLevel::INFO, [] (zenkit::LogLevel lvl, const char* cat, const char* message) {
    (void)cat;
    switch (lvl) {
      case zenkit::LogLevel::ERROR:
        Tempest::Log::e("[zenkit] ", message);
        break;
      case zenkit::LogLevel::WARNING:
        Tempest::Log::e("[zenkit] ", message);
        break;
      case zenkit::LogLevel::INFO:
        Tempest::Log::i("[zenkit] ", message);
        break;
      case zenkit::LogLevel::DEBUG:
      case zenkit::LogLevel::TRACE:
        Tempest::Log::d("[zenkit] ", message); // unused
        break;
      }
    });
  Dm_setLogger(DmLogLevel_INFO, [](void* ctx, DmLogLevel lvl, char const* msg) {
    switch (lvl) {
      case DmLogLevel_FATAL:
      case DmLogLevel_ERROR:
      case DmLogLevel_WARN:
        Tempest::Log::e("[dmusic] ", msg);
        break;
      case DmLogLevel_INFO:
        Tempest::Log::i("[dmusic] ", msg);
        break;
      case DmLogLevel_DEBUG:
      case DmLogLevel_TRACE:
        Tempest::Log::d("[dmusic] ", msg);
        break;
      }
    }, nullptr);

  Tempest::Log::i(appBuild);
  Workers::setThreadName("Main thread");

  CommandLine          cmd{argc,argv};
  auto                 api     = mkApi(cmd);
  const auto           gpuName = selectDevice(*api);
  CrashLog::setGpu(gpuName);

  Tempest::Device      device{*api,gpuName};
  CrashLog::setGpu(device.properties().name);

  Resources            resources{device};
  Gothic               gothic;
  GameMusic            music;
  gothic.setupGlobalScripts();

  MainWindow           wx(device);
  Tempest::Application app;
  return app.exec();
  }
