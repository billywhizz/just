#include "just.h"
#include "crypto.h"

namespace just {

void InitModules(Isolate* isolate, Local<ObjectTemplate> just) {
  vm::Init(isolate, just);
  tty::Init(isolate, just);
  fs::Init(isolate, just);
  sys::Init(isolate, just);
  http::Init(isolate, just);
  net::Init(isolate, just);
  loop::Init(isolate, just);
  crypto::Init(isolate, just);
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  signal(SIGPIPE, SIG_IGN);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  just::CreateIsolate(platform.get(), argc, argv, InitModules);
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  platform.reset();
  return 0;
}

}

int main(int argc, char** argv) {
  return just::main(argc, argv);
}