#include "just.h"
#include "thread.h"
#include "signal.h"
#include "udp.h"

namespace just {

namespace embedder {

void InitModules(Isolate* isolate, Local<ObjectTemplate> just) {
  just::InitModules(isolate, just);
  thread::Init(isolate, just, InitModules);
  signals::Init(isolate, just);
  udp::Init(isolate, just);
}

int Start(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  signal(SIGPIPE, SIG_IGN);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
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