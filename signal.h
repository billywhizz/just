#include "just.h"
#include <sys/signalfd.h>
#include <signal.h>

namespace just {

namespace signals {

void SignalFD(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> buf = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  sigset_t* set = static_cast<sigset_t*>(backing->Data());
  int flags = SFD_NONBLOCK | SFD_CLOEXEC;
  if (args.Length() > 1) {
    flags = args[1]->Int32Value(context).ToChecked();
  }
  int fd = signalfd(-1, set, flags);
  args.GetReturnValue().Set(Integer::New(isolate, fd));
}

void SigEmptySet(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<ArrayBuffer> buf = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  sigset_t* set = static_cast<sigset_t*>(backing->Data());
  int r = sigemptyset(set);
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void SigProcMask(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> buf = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  sigset_t* set = static_cast<sigset_t*>(backing->Data());
  int action = SIG_SETMASK;
  if (args.Length() > 1) {
    action = args[1]->Int32Value(context).ToChecked();
  }
  int direction = 0;
  if (args.Length() > 2) {
    direction = args[2]->Int32Value(context).ToChecked();
  }
  int r = 0;
  if (direction == 1) {
    r = pthread_sigmask(action, NULL, set);
  } else {
    r = pthread_sigmask(action, set, NULL);
  }
  if (r != 0) {
    args.GetReturnValue().Set(BigInt::New(isolate, r));
    return;
  }
  args.GetReturnValue().Set(BigInt::New(isolate, r));
}

void SigAddSet(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> buf = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  sigset_t* set = static_cast<sigset_t*>(backing->Data());
  int signum = args[1]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(BigInt::New(isolate, sigaddset(set, signum)));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> module = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, module, "sigprocmask", SigProcMask);
  SET_METHOD(isolate, module, "sigemptyset", SigEmptySet);
  SET_METHOD(isolate, module, "sigaddset", SigAddSet);
  SET_METHOD(isolate, module, "signalfd", SignalFD);
  SET_VALUE(isolate, module, "SFD_NONBLOCK", Integer::New(isolate, SFD_NONBLOCK));
  SET_VALUE(isolate, module, "SFD_CLOEXEC", Integer::New(isolate, SFD_CLOEXEC));
  SET_VALUE(isolate, module, "JUST_SIGSAVE", Integer::New(isolate, 1));
  SET_VALUE(isolate, module, "JUST_SIGLOAD", Integer::New(isolate, 0));
  SET_VALUE(isolate, module, "SIG_BLOCK", Integer::New(isolate, SIG_BLOCK));
  SET_VALUE(isolate, module, "SIG_SETMASK", Integer::New(isolate, SIG_SETMASK));
  SET_MODULE(isolate, target, "signal", module);
}

}

}
