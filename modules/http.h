#include "just.h"
#include <picohttpparser.h>
// #include <x86intrin.h>

namespace just {

namespace http {
typedef struct httpContext httpContext;

struct httpContext {
  int minor_version;
  int status;
  size_t status_message_len;
  size_t method_len;
  size_t path_len;
  uint32_t body_length;
  uint32_t body_bytes;
  uint16_t header_size;
  size_t num_headers;
  struct phr_header headers[JUST_MAX_HEADERS];
  const char* path;
  const char* method;
  const char* status_message;
};

httpContext state;

void GetUrl(const FunctionCallbackInfo<Value> &args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, state.path, 
    NewStringType::kNormal, state.path_len).ToLocalChecked());
}

void GetMethod(const FunctionCallbackInfo<Value> &args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, state.method, 
    NewStringType::kNormal, state.method_len).ToLocalChecked());
}

void GetStatusCode(const FunctionCallbackInfo<Value> &args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  args.GetReturnValue().Set(Integer::New(isolate, state.status));
}

void GetStatusMessage(const FunctionCallbackInfo<Value> &args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, state.status_message, 
    NewStringType::kNormal, state.status_message_len).ToLocalChecked());
}

void GetHeaders(const FunctionCallbackInfo<Value> &args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<Object> headers = Object::New(isolate);
  for (size_t i = 0; i < state.num_headers; i++) {
    struct phr_header* h = &state.headers[i];
    headers->Set(context, String::NewFromUtf8(isolate, h->name, 
      NewStringType::kNormal, h->name_len).ToLocalChecked(), 
      String::NewFromUtf8(isolate, h->value, NewStringType::kNormal, 
      h->value_len).ToLocalChecked()).Check();
  }
  args.GetReturnValue().Set(headers);
}

void GetRequest(const FunctionCallbackInfo<Value> &args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<Object> request = Object::New(isolate);
  request->Set(context, String::NewFromUtf8(isolate, 
    "minorVersion").ToLocalChecked(), Integer::New(isolate, 
    state.minor_version)).Check();
  request->Set(context, String::NewFromUtf8(isolate, 
    "url").ToLocalChecked(), String::NewFromUtf8(isolate, state.path, 
    NewStringType::kNormal, state.path_len).ToLocalChecked()).Check();
  request->Set(context, String::NewFromUtf8(isolate, 
    "method").ToLocalChecked(), String::NewFromUtf8(isolate, state.method, 
    NewStringType::kNormal, state.method_len).ToLocalChecked()).Check();
  Local<Object> headers = Object::New(isolate);
  for (size_t i = 0; i < state.num_headers; i++) {
    struct phr_header* h = &state.headers[i];
    headers->Set(context, String::NewFromUtf8(isolate, h->name, 
      NewStringType::kNormal, h->name_len).ToLocalChecked(), 
      String::NewFromUtf8(isolate, h->value, NewStringType::kNormal, 
      h->value_len).ToLocalChecked()).Check();
  }
  request->Set(context, String::NewFromUtf8(isolate, 
    "headers").ToLocalChecked(), headers).Check();
  args.GetReturnValue().Set(request);
}

void GetResponse(const FunctionCallbackInfo<Value> &args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<Object> request = Object::New(isolate);
  request->Set(context, String::NewFromUtf8(isolate, 
    "minorVersion").ToLocalChecked(), Integer::New(isolate, 
    state.minor_version)).Check();
  request->Set(context, String::NewFromUtf8(isolate, 
    "statusCode").ToLocalChecked(), Integer::New(isolate, 
    state.status)).Check();
  request->Set(context, String::NewFromUtf8(isolate, 
    "statusMessage").ToLocalChecked(), String::NewFromUtf8(isolate, 
    state.status_message, NewStringType::kNormal, 
    state.status_message_len).ToLocalChecked()).Check();
  Local<Object> headers = Object::New(isolate);
  for (size_t i = 0; i < state.num_headers; i++) {
    struct phr_header* h = &state.headers[i];
    headers->Set(context, String::NewFromUtf8(isolate, h->name, 
      NewStringType::kNormal, h->name_len).ToLocalChecked(), 
      String::NewFromUtf8(isolate, h->value, NewStringType::kNormal, 
      h->value_len).ToLocalChecked()).Check();
  }
  request->Set(context, String::NewFromUtf8(isolate, 
    "headers").ToLocalChecked(), headers).Check();
  args.GetReturnValue().Set(request);
}

void ParseRequest(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> buf = args[0].As<ArrayBuffer>();
  size_t bytes = args[1]->Int32Value(context).ToChecked();
  int argc = args.Length();
  size_t off = 0;
  if (argc > 2) {
    off = args[2]->Int32Value(context).ToChecked();
  }
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  char* next = (char*)backing->Data() + off;
  state.num_headers = JUST_MAX_HEADERS;
  ssize_t nread = phr_parse_request(next, bytes, (const char **)&state.method, 
    &state.method_len, (const char **)&state.path, &state.path_len, 
    &state.minor_version, state.headers, &state.num_headers, 0);
  args.GetReturnValue().Set(Integer::New(isolate, nread));
}
/*
void ParseRequest2(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> buf = args[0].As<ArrayBuffer>();
  size_t bytes = args[1]->Int32Value(context).ToChecked();
  int argc = args.Length();
  size_t off = 0;
  if (argc > 2) {
    off = args[2]->Int32Value(context).ToChecked();
  }
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  const char* next = (const char*)backing->Data() + off;
  const char* needle = "\r\n\r\n";
  uint32_t* offsets = (uint32_t*)backing->Data();
  int nlen = 4;
  size_t end = bytes + off;
  __m128i needle16 = _mm_loadu_si128((const __m128i *)needle);
  int count = 0;
  int orig = off;
  int r = 0;
  __m128i haystack16;
  while (off < end) {
    haystack16 = _mm_loadu_si128((const __m128i *)next);
    r = _mm_cmpestri(needle16, nlen, haystack16, 16, _SIDD_CMP_EQUAL_ORDERED | _SIDD_UBYTE_OPS);
    if (r < (16 - nlen)) {
      offsets[count++] = r + off + nlen;
    }
    off += 16 - nlen;
    next += 16 - nlen;
  }
  offsets[count] = orig + bytes;
  args.GetReturnValue().Set(Integer::New(isolate, count));
}
*/

void ParseResponse(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> buf = args[0].As<ArrayBuffer>();
  size_t bytes = args[1]->Int32Value(context).ToChecked();
  int argc = args.Length();
  size_t off = 0;
  if (argc > 2) {
    off = args[2]->Int32Value(context).ToChecked();
  }
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  char* next = (char*)backing->Data() + off;
  state.num_headers = JUST_MAX_HEADERS;
  ssize_t nread = phr_parse_response(next, bytes, &state.minor_version, 
    &state.status, (const char **)&state.status_message, 
    &state.status_message_len, state.headers, &state.num_headers, 0);
  args.GetReturnValue().Set(Integer::New(isolate, nread));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> http = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, http, "parseRequest", ParseRequest);
  SET_METHOD(isolate, http, "parseResponse", ParseResponse);
  SET_METHOD(isolate, http, "getUrl", GetUrl);
  SET_METHOD(isolate, http, "getStatusCode", GetStatusCode);
  SET_METHOD(isolate, http, "getStatusMessage", GetStatusMessage);
  SET_METHOD(isolate, http, "getMethod", GetMethod);
  SET_METHOD(isolate, http, "getHeaders", GetHeaders);
  SET_METHOD(isolate, http, "getRequest", GetRequest);
  SET_METHOD(isolate, http, "getResponse", GetResponse);
  SET_MODULE(isolate, target, "http", http);
}

}

}
