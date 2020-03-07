#include "just.h"
#include <mbedtls/ssl.h>
#include <mbedtls/version.h>

namespace just {

namespace tls {

void Hash(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> module = ObjectTemplate::New(isolate);
  char version_string[256];
  mbedtls_version_get_string_full(version_string);

  SET_VALUE(isolate, module, "version", String::NewFromUtf8(isolate, version_string).ToLocalChecked());
  SET_VALUE(isolate, module, "NONE", Integer::New(isolate, 0));
  SET_METHOD(isolate, module, "hash", Hash);
  SET_MODULE(isolate, target, "tls", module);
}

}

}
