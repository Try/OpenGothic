#include "haptics.h"

#ifdef __ANDROID__
#include <jni.h>

namespace {
JavaVM*   vm       = nullptr;
jclass    haptics  = nullptr;
jmethodID vibrateM = nullptr;
}

// Called by the system when the library is loaded, on a thread that knows the classes of the application
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* jvm, void*) {
  vm = jvm;
  JNIEnv* env = nullptr;
  if(vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)!=JNI_OK || env==nullptr)
    return JNI_VERSION_1_6;

  jclass cls = env->FindClass("org/opengothic/app/Haptics");
  if(cls!=nullptr) {
    haptics  = static_cast<jclass>(env->NewGlobalRef(cls));
    vibrateM = env->GetStaticMethodID(haptics,"vibrate","(II)V");
    env->DeleteLocalRef(cls);
    }
  if(env->ExceptionCheck())
    env->ExceptionClear();
  return JNI_VERSION_1_6;
  }

void Haptics::vibrate(int ms, int strength) {
  if(vm==nullptr || haptics==nullptr || vibrateM==nullptr)
    return;

  JNIEnv* env      = nullptr;
  bool    attached = false;
  if(vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)!=JNI_OK) {
    if(vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)
      return;
    attached = true;
    }

  env->CallStaticVoidMethod(haptics,vibrateM,jint(ms),jint(strength));
  if(env->ExceptionCheck())
    env->ExceptionClear();
  if(attached)
    vm->DetachCurrentThread();
  }
#else
void Haptics::vibrate(int, int) {
  }
#endif
