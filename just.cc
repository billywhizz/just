#include "just.h"
#include "thread.h"
#include "signal.h"
#include "udp.h"
#include "http.h"
#include "inspector.h"
#include "crypto.h"
#include "encode.h"

namespace just {

namespace embedder {

void InitModules(Isolate* isolate, Local<ObjectTemplate> just) {
  just::InitModules(isolate, just);
  thread::Init(isolate, just, InitModules);
  signals::Init(isolate, just);
  http::Init(isolate, just);
  udp::Init(isolate, just);
  inspector::Init(isolate, just);
  crypto::Init(isolate, just);
  encode::Init(isolate, just);
}

int Start(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  // have to block all signals we are interested in before we initialise
  // v8. if we don't then setting sigmask after initialization has no effect
  // must be something internal in v8 that uses the initial mask and resets to
  // that
  sigset_t set;
  int r = pthread_sigmask(SIG_SETMASK, NULL, &set);
  for (int i = 1; i < 64; i++) {
    r = sigaddset(&set, i);
  }
  r = pthread_sigmask(SIG_SETMASK, &set, NULL);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  r = sigemptyset(&set);
  // unblock signals - i need to find out why we have to do this!
  r = pthread_sigmask(SIG_SETMASK, &set, NULL);
  signal(SIGPIPE, SIG_IGN);
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  just::CreateIsolate(argc, argv, InitModules);
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  platform.reset();
  return 0;
}

}

}

int main(int argc, char** argv) {
  return just::embedder::Start(argc, argv);
}