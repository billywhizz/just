#include "just.h"
#include <pthread.h>

namespace just {

namespace thread {

struct threadContext {
  int argc;
  char** argv;
  char* source;
  struct iovec buf;
  int fd;
  unsigned int source_len;
};

static InitModulesCallback initModules;

void* startThread(void *data) {
  threadContext* ctx = (threadContext*)data;
  just::CreateIsolate(ctx->argc, ctx->argv, initModules, ctx->source, ctx->source_len, &ctx->buf, ctx->fd);
  free(ctx->source);
  free(ctx);
  return NULL;
}

void Spawn(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value source(isolate, args[0]);
  Local<Context> context = isolate->GetCurrentContext();
  threadContext* ctx = (threadContext*)calloc(1, sizeof(threadContext));
  ctx->argc = 1;
  ctx->argv = new char*[2];
  ctx->argv[1] = NULL;
	ctx->source = (char*)calloc(1, source.length());
  memcpy(ctx->source, *source, source.length());
  ctx->source_len = source.length();
  int argc = args.Length();
  ctx->buf.iov_len = 0;
  ctx->fd = 0;
  if (argc > 1) {
    Local<SharedArrayBuffer> ab = args[1].As<SharedArrayBuffer>();
    std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
    ctx->buf.iov_base = backing->Data();
    ctx->buf.iov_len = backing->ByteLength();
  }
  if (argc > 2) {
    ctx->fd = args[2]->Int32Value(context).ToChecked();
  }
  if (argc > 3) {
    String::Utf8Value name(isolate, args[3]);
    ctx->argv[0] = (char*)calloc(1, name.length());
    memcpy(ctx->argv[0], *name, name.length());
  } else {
    ctx->argv[0] = (char*)calloc(1, 6);
    strncpy(ctx->argv[0], "thread", 6);
  }
  pthread_t tid;
	int r = pthread_create(&tid, NULL, startThread, ctx);
  if (r != 0) {
    args.GetReturnValue().Set(BigInt::New(isolate, r));
    return;
  }
  args.GetReturnValue().Set(BigInt::New(isolate, tid));
}

void Join(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<BigInt> bi = args[0]->ToBigInt(context).ToLocalChecked();
  bool lossless = true;
  pthread_t tid = (pthread_t)bi->Uint64Value(&lossless);
  void* tret;
  int r = pthread_join(tid, &tret);
  if (r != 0) {
    args.GetReturnValue().Set(BigInt::New(isolate, r));
    return;
  }
  args.GetReturnValue().Set(BigInt::New(isolate, (long)tret));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target, InitModulesCallback InitModules) {
  Local<ObjectTemplate> module = ObjectTemplate::New(isolate);
  initModules = InitModules;
  SET_METHOD(isolate, module, "spawn", Spawn);
  SET_METHOD(isolate, module, "join", Join);
  SET_MODULE(isolate, target, "thread", module);
}

}

}
