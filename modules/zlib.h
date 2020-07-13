#include "just.h"
#include <zconf.h>
#include <zlib.h>

namespace just {

namespace zlib {

#define Z_MIN_CHUNK 64
#define Z_MAX_CHUNK std::numeric_limits<double>::infinity()
#define Z_DEFAULT_CHUNK (16 * 1024)
#define Z_MIN_MEMLEVEL 1
#define Z_MAX_MEMLEVEL 9
#define Z_DEFAULT_MEMLEVEL 8
#define Z_MIN_LEVEL -1
#define Z_MAX_LEVEL 9
#define Z_DEFAULT_LEVEL Z_DEFAULT_COMPRESSION
#define Z_MIN_WINDOWBITS 8
#define Z_MAX_WINDOWBITS 15
#define Z_DEFAULT_WINDOW_BITS 15

enum zlib_mode {
  NONE,
  DEFLATE,
  INFLATE,
  GZIP,
  GUNZIP,
  DEFLATERAW,
  INFLATERAW,
  UNZIP
};

void FreeMemory(void* buf, size_t length, void* data) {
  //fprintf(stderr, "free: %lu\n", length);
}

void Crc32(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  unsigned int len = args[1]->Uint32Value(context).ToChecked();
  unsigned long crc = args[2]->Uint32Value(context).ToChecked();
  args.GetReturnValue().Set(v8::Uint32::New(isolate, crc32(crc, 
    (const uint8_t *)backing->Data(), len)));
}

void WriteDeflate(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  z_stream* stream = (z_stream*)ab->GetAlignedPointerFromInternalField(1);
  int argc = args.Length();
  unsigned int flush = Z_NO_FLUSH;
  unsigned int len = args[1]->Uint32Value(context).ToChecked();
  if (argc > 2) {
    flush = args[2]->Uint32Value(context).ToChecked();
  }
  unsigned char* next_in = stream->next_in;
  unsigned int avail_in = stream->avail_in;
  unsigned char* next_out = stream->next_out;
  unsigned int avail_out = stream->avail_out;
  stream->avail_in = len;
  int err = deflate(stream, flush);
  if (err < 0) {
    args.GetReturnValue().Set(Integer::New(isolate, err));
    return;
  }
  unsigned int bytes = avail_out - stream->avail_out;
  stream->next_in = next_in;
  stream->avail_in = avail_in;
  stream->next_out = next_out;
  stream->avail_out = avail_out;
  args.GetReturnValue().Set(Integer::New(isolate, bytes));
}

void WriteInflate(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> outab = args[0].As<ArrayBuffer>();
  z_stream* stream = (z_stream*)outab->GetAlignedPointerFromInternalField(1);
  Local<ArrayBuffer> inab = args[1].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> in = inab->GetBackingStore();
  unsigned int flush = Z_NO_FLUSH;
  unsigned int off = args[2]->Uint32Value(context).ToChecked();
  unsigned int len = args[3]->Uint32Value(context).ToChecked();
  Local<Array> state = args[4].As<Array>();
  int argc = args.Length();
  if (argc > 5) {
    flush = args[5]->Uint32Value(context).ToChecked();
  }
  stream->next_in = (unsigned char*)in->Data() + off;
  stream->avail_in = len;
  unsigned int avail_in = stream->avail_in;
  unsigned char* next_out = stream->next_out;
  unsigned int avail_out = stream->avail_out;
  int err = inflate(stream, flush);
  unsigned int byteswritten = avail_out - stream->avail_out;
  unsigned int bytesread = avail_in - stream->avail_in;
  stream->next_out = next_out;
  stream->avail_out = avail_out;
  state->Set(context, 0, Integer::New(isolate, bytesread)).Check();
  state->Set(context, 1, Integer::New(isolate, byteswritten)).Check();
  args.GetReturnValue().Set(Integer::New(isolate, err));
}

void EndDeflate(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  z_stream* stream = (z_stream*)ab->GetAlignedPointerFromInternalField(1);
  int r = deflateEnd(stream);
  if (r < 0) {
    args.GetReturnValue().Set(Integer::New(isolate, 0));
    return;
  }
  args.GetReturnValue().Set(Integer::New(isolate, stream->adler));
}

void EndInflate(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  z_stream* stream = (z_stream*)ab->GetAlignedPointerFromInternalField(1);
  int r = inflateEnd(stream);
  if (r < 0) {
    args.GetReturnValue().Set(Integer::New(isolate, 0));
    return;
  }
  args.GetReturnValue().Set(Integer::New(isolate, stream->adler));
}

void CreateDeflate(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  z_stream* stream = (z_stream*)calloc(1, sizeof(z_stream));
  int argc = args.Length();
  unsigned int compression = Z_DEFAULT_COMPRESSION;
  unsigned int err = 0;
  int windowbits = Z_DEFAULT_WINDOW_BITS;
  Local<ArrayBuffer> inab = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> in = inab->GetBackingStore();
  int outlen = 4096;
  if (argc > 1) {
    outlen = args[1]->Uint32Value(context).ToChecked();
  }
  void* chunk = calloc(1, outlen);
  std::unique_ptr<BackingStore> out =
      ArrayBuffer::NewBackingStore(chunk, outlen, FreeMemory, nullptr);
  Local<ArrayBuffer> outab = ArrayBuffer::New(isolate, std::move(out));
  if (argc > 2) compression = args[2]->Uint32Value(context).ToChecked();
  if (argc > 3) windowbits = args[3]->Int32Value(context).ToChecked();
  err = deflateInit2(stream, compression, Z_DEFLATED, windowbits, 
    Z_DEFAULT_MEMLEVEL, Z_DEFAULT_STRATEGY);
  stream->next_in = reinterpret_cast<Bytef *>(in->Data());
  stream->avail_in = in->ByteLength();
  stream->next_out = reinterpret_cast<Bytef *>(chunk);
  stream->avail_out = outlen;
  outab->SetAlignedPointerInInternalField(1, stream);
  args.GetReturnValue().Set(outab);
}

void CreateInflate(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  z_stream* stream = (z_stream*)calloc(1, sizeof(z_stream));
  int argc = args.Length();
  int windowbits = Z_DEFAULT_WINDOW_BITS;
  unsigned int err = 0;
  Local<ArrayBuffer> inab = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> in = inab->GetBackingStore();
  int outlen = 4096;
  if (argc > 1) {
    outlen = args[1]->Uint32Value(context).ToChecked();
  }
  void* chunk = calloc(1, outlen);
  std::unique_ptr<BackingStore> out =
      ArrayBuffer::NewBackingStore(chunk, outlen, FreeMemory, nullptr);
  Local<ArrayBuffer> outab = ArrayBuffer::New(isolate, std::move(out));
  if (argc > 2) windowbits = args[2]->Int32Value(context).ToChecked();
  err = inflateInit2(stream, windowbits);
  stream->next_in = reinterpret_cast<Bytef *>(in->Data());
  stream->avail_in = in->ByteLength();
  stream->next_out = reinterpret_cast<Bytef *>(chunk);
  stream->avail_out = outlen;
  outab->SetAlignedPointerInInternalField(1, stream);
  args.GetReturnValue().Set(outab);
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> module = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, module, "createDeflate", CreateDeflate);
  SET_METHOD(isolate, module, "createInflate", CreateInflate);
  SET_METHOD(isolate, module, "crc32", Crc32);
  SET_METHOD(isolate, module, "writeDeflate", WriteDeflate);
  SET_METHOD(isolate, module, "writeInflate", WriteInflate);
  SET_METHOD(isolate, module, "endDeflate", EndDeflate);
  SET_METHOD(isolate, module, "endInflate", EndInflate);
  SET_VALUE(isolate, module, "Z_NO_FLUSH", Integer::New(isolate, 
    Z_NO_FLUSH));
  SET_VALUE(isolate, module, "Z_FULL_FLUSH", Integer::New(isolate, 
    Z_FULL_FLUSH));
  SET_VALUE(isolate, module, "Z_FINISH", Integer::New(isolate, Z_FINISH));
  SET_VALUE(isolate, module, "Z_DEFAULT_COMPRESSION", Integer::New(isolate, 
    Z_DEFAULT_COMPRESSION));
  SET_VALUE(isolate, module, "Z_BEST_COMPRESSION", Integer::New(isolate, 
    Z_BEST_COMPRESSION));
  SET_VALUE(isolate, module, "Z_NO_COMPRESSION", Integer::New(isolate, 
    Z_NO_COMPRESSION));
  SET_VALUE(isolate, module, "Z_BEST_SPEED", Integer::New(isolate, 
    Z_BEST_SPEED));
  SET_VALUE(isolate, module, "Z_DEFAULT_WINDOW_BITS", Integer::New(isolate, 
    Z_DEFAULT_WINDOW_BITS));
  SET_VALUE(isolate, module, "Z_OK", Integer::New(isolate, Z_OK));
  SET_VALUE(isolate, module, "Z_STREAM_END", Integer::New(isolate, 
    Z_STREAM_END));
  SET_VALUE(isolate, module, "Z_NEED_DICT", Integer::New(isolate, Z_NEED_DICT));
  SET_VALUE(isolate, module, "Z_ERRNO", Integer::New(isolate, Z_ERRNO));
  SET_VALUE(isolate, module, "Z_STREAM_ERROR", Integer::New(isolate, 
    Z_STREAM_ERROR));
  SET_VALUE(isolate, module, "Z_DATA_ERROR", Integer::New(isolate, 
    Z_DATA_ERROR));
  SET_VALUE(isolate, module, "Z_MEM_ERROR", Integer::New(isolate, Z_MEM_ERROR));
  SET_VALUE(isolate, module, "Z_BUF_ERROR", Integer::New(isolate, Z_BUF_ERROR));
  SET_VALUE(isolate, module, "Z_VERSION_ERROR", Integer::New(isolate, 
    Z_VERSION_ERROR));
  SET_MODULE(isolate, target, "zlib", module);
}

}

}
