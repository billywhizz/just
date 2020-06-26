#include "just.h"
#ifndef JUST_MIN
#include "modules/thread.h"
#include "modules/signal.h"
#include "modules/udp.h"
#include "modules/zlib.h"
#include "modules/tls.h"
// need these four for inspector
#include "modules/http.h"
#include "modules/inspector.h"
#include "modules/crypto.h"
#include "modules/encode.h"
#endif

namespace just {

namespace embedder {

void InitModules(Isolate* isolate, Local<ObjectTemplate> just) {
  // c++ runtime
  // initialize the default modules
  just::InitModules(isolate, just);
#ifndef JUST_MIN
  // required by inspector
  http::Init(isolate, just);
  inspector::Init(isolate, just);
  crypto::Init(isolate, just);
  encode::Init(isolate, just);
  // miscellaneous modules
  thread::Init(isolate, just, InitModules);
  signals::Init(isolate, just);
  udp::Init(isolate, just);
  zlib::Init(isolate, just);
  tls::Init(isolate, just);
  // required for repl capability
  just_builtins_add("repl", lib_repl_js, lib_repl_js_len);
  // required for debugging onlu
  just_builtins_add("inspector", lib_inspector_js, lib_inspector_js_len);
  just_builtins_add("websocket", lib_websocket_js, lib_websocket_js_len);
  // miscellaneous libs
  just_builtins_add("fs", lib_fs_js, lib_fs_js_len);
  just_builtins_add("wasm", lib_wasm_js, lib_wasm_js_len);
  just_builtins_add("libwabt", lib_libwabt_js, lib_libwabt_js_len);
#endif
  // main script js
  just_builtins_add("just", just_js, just_js_len);
  // required by main
  just_builtins_add("path", lib_path_js, lib_path_js_len);
  just_builtins_add("loop", lib_loop_js, lib_loop_js_len);
  // if you omit this the will only be possible to require builtins
  just_builtins_add("require", lib_require_js, lib_require_js_len);
}

int Start(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  // have to block all signals we are interested in before we initialise
  // v8. if we don't then setting sigmask after initialization has no effect
  // must be something internal in v8 that uses the initial mask and resets to
  // that
  sigset_t set;
  int r = 0;
  r = pthread_sigmask(SIG_SETMASK, NULL, &set);
  if (r != 0) return r;
  for (int i = 1; i < 64; i++) {
    r = sigaddset(&set, i);
    if (r != 0) return r;
  }
  r = pthread_sigmask(SIG_SETMASK, &set, NULL);
  if (r != 0) return r;
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  r = sigemptyset(&set);
  if (r != 0) return r;
  // unblock signals - i need to find out why we have to do this!
  r = pthread_sigmask(SIG_SETMASK, &set, NULL);
  if (r != 0) return r;
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
