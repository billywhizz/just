#include "just.h"
#include <mbedtls/md4.h>
#include <mbedtls/md5.h>
#include <mbedtls/ripemd160.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/md.h>
#include <mbedtls/version.h>

namespace just {

namespace crypto {

void Hash(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  const mbedtls_md_info_t* algorithm = mbedtls_md_info_from_type((mbedtls_md_type_t)args[0]->Uint32Value(context).ToChecked());
  Local<ArrayBuffer> absource = args[1].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> source = absource->GetBackingStore();
  Local<ArrayBuffer> abdest = args[2].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> dest = abdest->GetBackingStore();
  int len = source->ByteLength();
  if (args.Length() > 3) {
    len = args[3]->Uint32Value(context).ToChecked();
  }
  //todo: buffer overrun
  int ret = mbedtls_md(algorithm, (const unsigned char*)source->Data(), len, (unsigned char*)dest->Data());
  args.GetReturnValue().Set(Integer::New(isolate, ret));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> module = ObjectTemplate::New(isolate);
  char version_string[256];
  mbedtls_version_get_string_full(version_string);
  SET_VALUE(isolate, module, "version", String::NewFromUtf8(isolate, version_string).ToLocalChecked());

  SET_VALUE(isolate, module, "NONE", Integer::New(isolate, MBEDTLS_MD_NONE));
  SET_VALUE(isolate, module, "MD2", Integer::New(isolate, MBEDTLS_MD_MD2));
  SET_VALUE(isolate, module, "MD4", Integer::New(isolate, MBEDTLS_MD_MD4));
  SET_VALUE(isolate, module, "MD5", Integer::New(isolate, MBEDTLS_MD_MD5));
  SET_VALUE(isolate, module, "SHA1", Integer::New(isolate, MBEDTLS_MD_SHA1));
  SET_VALUE(isolate, module, "SHA224", Integer::New(isolate, MBEDTLS_MD_SHA224));
  SET_VALUE(isolate, module, "SHA256", Integer::New(isolate, MBEDTLS_MD_SHA256));
  SET_VALUE(isolate, module, "SHA384", Integer::New(isolate, MBEDTLS_MD_SHA384));
  SET_VALUE(isolate, module, "SHA512", Integer::New(isolate, MBEDTLS_MD_SHA512));
  SET_VALUE(isolate, module, "RIPEMD160", Integer::New(isolate, MBEDTLS_MD_RIPEMD160));

  SET_METHOD(isolate, module, "hash", Hash);
  SET_MODULE(isolate, target, "crypto", module);
}

}

}
