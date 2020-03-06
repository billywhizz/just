#include "just.h"

namespace just {

namespace ${name} {

void Hello(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  args.GetReturnValue().Set(Integer::New(isolate, 0));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> module = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, module, "hello", Hello);
  SET_MODULE(isolate, target, "${name}", module);
}

}

}
