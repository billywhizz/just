#include "just.h"
#include "modules/thread.h"
#include "modules/signal.h"
#include "modules/udp.h"
#include "modules/zlib.h"
// need these four for inspector
#include "modules/http.h"
#include "modules/inspector.h"
#include "modules/crypto.h"
#include "modules/encode.h"

namespace just {

namespace embedder {

void InitModules(Isolate* isolate, Local<ObjectTemplate> just) {
  just::InitModules(isolate, just);
  thread::Init(isolate, just, InitModules);
  signals::Init(isolate, just);
  udp::Init(isolate, just);
// need these four for inspector
  http::Init(isolate, just);
  inspector::Init(isolate, just);
  crypto::Init(isolate, just);
  encode::Init(isolate, just);
  zlib::Init(isolate, just);
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