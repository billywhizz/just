#include "just.h"
#include "crypto.h"
#include "thread.h"
#include "builtins.h"

namespace just {

namespace embedder {

void InitModules(Isolate* isolate, Local<ObjectTemplate> just) {
  just::InitModules(isolate, just);
  crypto::Init(isolate, just);
  thread::Init(isolate, just);
}

int Start(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  signal(SIGPIPE, SIG_IGN);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  just::CreateIsolate(argc, argv, InitModules, just_js, just_js_len, NULL, 0);
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