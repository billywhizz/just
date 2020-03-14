#include "just.h"
#include <openssl/ssl.h>
#include <openssl/opensslv.h>

namespace just {

namespace tls {

void Hash(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  // Local<Context> context = isolate->GetCurrentContext();
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> module = ObjectTemplate::New(isolate);
  SET_VALUE(isolate, module, "version", String::NewFromUtf8(isolate, OPENSSL_VERSION_TEXT).ToLocalChecked());
  SET_VALUE(isolate, module, "NONE", Integer::New(isolate, 0));
  SET_METHOD(isolate, module, "hash", Hash);
  SET_MODULE(isolate, target, "tls", module);
}

}

}
