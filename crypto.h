#include "just.h"

namespace just {

namespace crypto {

void Digest(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  //Local<Context> context = isolate->GetCurrentContext();
  args.GetReturnValue().Set(Integer::New(isolate, 0));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> module = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, module, "digest", Digest);
  SET_MODULE(isolate, target, "crypto", module);
}

}

}
