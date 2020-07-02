#ifndef JUST_H
#define JUST_H

#include <v8.h>
#include <libplatform/libplatform.h>
#include <unistd.h>
#include <limits.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <sys/utsname.h>
#include <gnu/libc-version.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <map>
#include "builtins.h"

#define JUST_MAX_HEADERS 32
#define JUST_MICROS_PER_SEC 1e6
#define JUST_VERSION "0.0.1"

namespace just {

using v8::String;
using v8::NewStringType;
using v8::Local;
using v8::Isolate;
using v8::Context;
using v8::ObjectTemplate;
using v8::FunctionCallbackInfo;
using v8::Function;
using v8::Object;
using v8::Value;
using v8::Persistent;
using v8::MaybeLocal;
using v8::Module;
using v8::TryCatch;
using v8::Message;
using v8::StackTrace;
using v8::StackFrame;
using v8::HandleScope;
using v8::Integer;
using v8::BigInt;
using v8::FunctionTemplate;
using v8::ScriptOrigin;
using v8::True;
using v8::False;
using v8::ScriptCompiler;
using v8::ArrayBuffer;
using v8::Array;
using v8::Maybe;
using v8::ArrayBufferCreationMode;
using v8::HeapStatistics;
using v8::Float64Array;
using v8::HeapSpaceStatistics;
using v8::BigUint64Array;
using v8::Int32Array;
using v8::Exception;
using v8::Signature;
using v8::FunctionCallback;
using v8::ScriptOrModule;
using v8::Script;
using v8::MicrotasksScope;
using v8::Platform;
using v8::V8;
using v8::BackingStore;
using v8::BackingStoreDeleterCallback;
using v8::SharedArrayBuffer;
using v8::PromiseRejectMessage;
using v8::Promise;
using v8::PromiseRejectEvent;

typedef void    (*InitModulesCallback) (Isolate*, Local<ObjectTemplate>);

struct builtin {
  unsigned int size;
  const char* source;
};

std::map<std::string, builtin*> builtins;

void just_builtins_add(const char* name, const char* source, 
  unsigned int size) {
  struct builtin* b = new builtin();
  b->size = size;
  b->source = source;
  builtins[name] = b;
}

inline ssize_t process_memory_usage() {
  char buf[1024];
  const char* s = NULL;
  ssize_t n = 0;
  long val = 0;
  int fd = 0;
  int i = 0;
  do {
    fd = open("/proc/self/stat", O_RDONLY);
  } while (fd == -1 && errno == EINTR);
  if (fd == -1) return (ssize_t)errno;
  do
    n = read(fd, buf, sizeof(buf) - 1);
  while (n == -1 && errno == EINTR);
  close(fd);
  if (n == -1)
    return (ssize_t)errno;
  buf[n] = '\0';
  s = strchr(buf, ' ');
  if (s == NULL)
    goto err;
  s += 1;
  if (*s != '(')
    goto err;
  s = strchr(s, ')');
  if (s == NULL)
    goto err;
  for (i = 1; i <= 22; i++) {
    s = strchr(s + 1, ' ');
    if (s == NULL)
      goto err;
  }
  errno = 0;
  val = strtol(s, NULL, 10);
  if (errno != 0)
    goto err;
  if (val < 0)
    goto err;
  return val * getpagesize();
err:
  return 0;
}

inline uint64_t hrtime() {
  struct timespec t;
  clock_t clock_id = CLOCK_MONOTONIC;
  if (clock_gettime(clock_id, &t))
    return 0;
  return t.tv_sec * (uint64_t) 1e9 + t.tv_nsec;
}

inline void SET_METHOD(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, FunctionCallback callback) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kNormal).ToLocalChecked(), 
    FunctionTemplate::New(isolate, callback));
}

inline void SET_MODULE(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, Local<ObjectTemplate> module) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kNormal).ToLocalChecked(), 
    module);
}

inline void SET_VALUE(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, Local<Value> value) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kNormal).ToLocalChecked(), 
    value);
}

Local<String> ReadFile(Isolate *isolate, const char *name) {
  FILE *file = fopen(name, "rb");
  if (file == NULL) {
    isolate->ThrowException(Exception::Error(
      String::NewFromUtf8Literal(isolate, 
      "Bad File", NewStringType::kNormal)));
    return Local<String>();
  }
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);
  char chars[size + 1];
  chars[size] = '\0';
  for (size_t i = 0; i < size;) {
    i += fread(&chars[i], 1, size - i, file);
    if (ferror(file)) {
      fclose(file);
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, 
        "Read Error", NewStringType::kNormal).ToLocalChecked()));
      return Local<String>();
    }
  }
  fclose(file);
  Local<String> result = String::NewFromUtf8(isolate, chars, 
    NewStringType::kNormal, static_cast<int>(size)).ToLocalChecked();
  return result;
}

MaybeLocal<Module> OnModuleInstantiate(Local<Context> context, 
  Local<String> specifier, Local<Module> referrer) {
  HandleScope handle_scope(context->GetIsolate());
  return MaybeLocal<Module>();
}

void PrintStackTrace(Isolate* isolate, const TryCatch& try_catch) {
  HandleScope handleScope(isolate);
  Local<Value> exception = try_catch.Exception();
  Local<Message> message = try_catch.Message();
  Local<StackTrace> stack = message->GetStackTrace();
  String::Utf8Value ex(isolate, exception);
  Local<Value> scriptName = message->GetScriptResourceName();
  String::Utf8Value scriptname(isolate, scriptName);
  Local<Context> context = isolate->GetCurrentContext();
  int linenum = message->GetLineNumber(context).FromJust();
  fprintf(stderr, "%s in %s on line %i\n", *ex, *scriptname, linenum);
  if (stack.IsEmpty()) return;
  for (int i = 0; i < stack->GetFrameCount(); i++) {
    Local<StackFrame> stack_frame = stack->GetFrame(isolate, i);
    Local<String> functionName = stack_frame->GetFunctionName();
    Local<String> scriptName = stack_frame->GetScriptName();
    String::Utf8Value fn_name_s(isolate, functionName);
    String::Utf8Value script_name(isolate, scriptName);
    const int line_number = stack_frame->GetLineNumber();
    const int column = stack_frame->GetColumn();
    if (stack_frame->IsEval()) {
      if (stack_frame->GetScriptId() == Message::kNoScriptIdInfo) {
        fprintf(stderr, "    at [eval]:%i:%i\n", line_number, column);
      } else {
        fprintf(stderr, "    at [eval] (%s:%i:%i)\n", *script_name,
          line_number, column);
      }
      break;
    }
    if (fn_name_s.length() == 0) {
      fprintf(stderr, "    at %s:%i:%i\n", *script_name, line_number, column);
    } else {
      fprintf(stderr, "    at %s (%s:%i:%i)\n", *fn_name_s, *script_name,
        line_number, column);
    }
  }
  fflush(stderr);
}

void Print(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  if (args[0].IsEmpty()) return;
  String::Utf8Value str(args.GetIsolate(), args[0]);
  int endline = 1;
  if (args.Length() > 1) {
    endline = static_cast<int>(args[1]->BooleanValue(isolate));
  }
  const char *cstr = *str;
  if (endline == 1) {
    fprintf(stdout, "%s\n", cstr);
  } else {
    fprintf(stdout, "%s", cstr);
  }
}

void Error(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  if (args[0].IsEmpty()) return;
  String::Utf8Value str(args.GetIsolate(), args[0]);
  int endline = 1;
  if (args.Length() > 1) {
    endline = static_cast<int>(args[1]->BooleanValue(isolate));
  }
  const char *cstr = *str;
  if (endline == 1) {
    fprintf(stderr, "%s\n", cstr);
  } else {
    fprintf(stderr, "%s", cstr);
  }
}

namespace vm {

void CompileScript(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  TryCatch try_catch(isolate);
  Local<String> source = args[0].As<String>();
  Local<String> path = args[1].As<String>();
  Local<Array> params_buf = args[2].As<Array>();
  Local<Array> context_extensions_buf;
  context_extensions_buf = args[3].As<Array>();
  std::vector<Local<String>> params;
  if (!params_buf.IsEmpty()) {
    for (uint32_t n = 0; n < params_buf->Length(); n++) {
      Local<Value> val;
      if (!params_buf->Get(context, n).ToLocal(&val)) return;
      params.push_back(val.As<String>());
    }
  }
  std::vector<Local<Object>> context_extensions;
  if (!context_extensions_buf.IsEmpty()) {
    for (uint32_t n = 0; n < context_extensions_buf->Length(); n++) {
      Local<Value> val;
      if (!context_extensions_buf->Get(context, n).ToLocal(&val)) return;
      context_extensions.push_back(val.As<Object>());
    }
  }
  ScriptOrigin baseorigin(path, // resource name
    Integer::New(isolate, 0), // line offset
    Integer::New(isolate, 0),  // column offset
    True(isolate));
  Context::Scope scope(context);
  ScriptCompiler::Source basescript(source, baseorigin);
  MaybeLocal<Function> maybe_fn = ScriptCompiler::CompileFunctionInContext(
    context, &basescript, params.size(), params.data(), 0, nullptr, 
    ScriptCompiler::kEagerCompile);
  if (maybe_fn.IsEmpty()) {
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      try_catch.ReThrow();
    }
    return;
  }
  Local<Function> fn = maybe_fn.ToLocalChecked();
  args.GetReturnValue().Set(fn);
}

void RunModule(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  TryCatch try_catch(isolate);
  Local<String> source = args[0].As<String>();
  Local<String> path = args[1].As<String>();
  ScriptOrigin baseorigin(path, // resource name
    Integer::New(isolate, 0), // line offset
    Integer::New(isolate, 0),  // column offset
    False(isolate), // is shared cross-origin
    Local<Integer>(),  // script id
    Local<Value>(), // source map url
    False(isolate), // is opaque
    False(isolate), // is wasm
    True(isolate)); // is module
  ScriptCompiler::Source basescript(source, baseorigin);
  Local<Module> module;
  bool ok = ScriptCompiler::CompileModule(isolate, 
    &basescript).ToLocal(&module);
  if (!ok) {
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      try_catch.ReThrow();
    }
    return;
  }
  Maybe<bool> ok2 = module->InstantiateModule(context, OnModuleInstantiate);
  if (ok2.IsNothing()) {
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      try_catch.ReThrow();
    }
    return;
  }
  MaybeLocal<Value> result = module->Evaluate(context);
  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    try_catch.ReThrow();
    return;
  }
  args.GetReturnValue().Set(result.ToLocalChecked());
}

void Builtin(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  String::Utf8Value name(isolate, args[0]);
  builtin* b = builtins[*name];
  if (b == nullptr) {
    args.GetReturnValue().Set(Null(isolate));
    return;
  }
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, b->source, 
    NewStringType::kNormal, b->size).ToLocalChecked());
}

void RunScript(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  TryCatch try_catch(isolate);
  Local<String> source = args[0].As<String>();
  Local<String> path = args[1].As<String>();
  ScriptOrigin baseorigin(path, // resource name
    Integer::New(isolate, 0), // line offset
    Integer::New(isolate, 0),  // column offset
    False(isolate), // is shared cross-origin
    Local<Integer>(),  // script id
    Local<Value>(), // source map url
    False(isolate), // is opaque
    False(isolate), // is wasm
    False(isolate)); // is module
  Local<Script> script;
  ScriptCompiler::Source basescript(source, baseorigin);
  bool ok = ScriptCompiler::Compile(context, &basescript).ToLocal(&script);
  if (!ok) {
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      try_catch.ReThrow();
    }
    return;
  }
  MaybeLocal<Value> result = script->Run(context);
  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    try_catch.ReThrow();
    return;
  }
  args.GetReturnValue().Set(result.ToLocalChecked());
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> vm = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, vm, "compile", just::vm::CompileScript);
  SET_METHOD(isolate, vm, "runModule", just::vm::RunModule);
  SET_METHOD(isolate, vm, "runScript", just::vm::RunScript);
  SET_METHOD(isolate, vm, "builtin", just::vm::Builtin);
  SET_MODULE(isolate, target, "vm", vm);
}

}

namespace sys {

void WaitPID(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> ab = args[0].As<Int32Array>()->Buffer();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  int *fields = static_cast<int *>(backing->Data());
  int pid = -1;
  if (args.Length() > 1) {
    pid = args[0]->IntegerValue(context).ToChecked();
  }
  fields[1] = waitpid(pid, &fields[0], WNOHANG); 
  args.GetReturnValue().Set(args[0]);
}

void Spawn(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  String::Utf8Value filePath(isolate, args[0]);
  String::Utf8Value cwd(isolate, args[1]);
  Local<Array> arguments = args[2].As<Array>();
  int fds[3];
  fds[0] = args[3]->IntegerValue(context).ToChecked();
  fds[1] = args[4]->IntegerValue(context).ToChecked();
  fds[2] = args[5]->IntegerValue(context).ToChecked();
  int len = arguments->Length();
  char* argv[len + 2];
  int written = 0;
  argv[0] = (char*)calloc(1, filePath.length());
  memcpy(argv[0], *filePath, filePath.length());
  for (int i = 0; i < len; i++) {
    Local<String> val = 
      arguments->Get(context, i).ToLocalChecked().As<v8::String>();
    argv[i + 1] = (char*)calloc(1, val->Length());
    val->WriteUtf8(isolate, argv[i + 1], val->Length(), &written, 
      v8::String::HINT_MANY_WRITES_EXPECTED | v8::String::NO_NULL_TERMINATION);
  }
  argv[len + 1] = NULL;
  pid_t pid = fork();
  if (pid == -1) {
    perror("error forking");
    args.GetReturnValue().Set(Integer::New(isolate, pid));
    return;
  }
  if (pid == 0) {
    close(0);
    close(1);
    close(2);
    dup2(fds[0], 0);
    dup2(fds[1], 1);
    dup2(fds[2], 2);
    execvp(*filePath, argv);
    perror("error launching child process");
    exit(127);
  } else {
    close(fds[0]);
    close(fds[1]);
    close(fds[2]);
    args.GetReturnValue().Set(Integer::New(isolate, pid));
    return;
  }
}

void HRTime(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<BigUint64Array> b64 = args[0].As<BigUint64Array>();
  Local<ArrayBuffer> ab = b64->Buffer();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  uint64_t *fields = static_cast<uint64_t *>(backing->Data());
  fields[0] = hrtime();
  args.GetReturnValue().Set(b64);
}

void RunMicroTasks(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  isolate->PerformMicrotaskCheckpoint();
  //MicrotasksScope::PerformCheckpoint(isolate);
}

void EnqueueMicrotask(const FunctionCallbackInfo<Value>& args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  isolate->EnqueueMicrotask(args[0].As<Function>());
}

void Exit(const FunctionCallbackInfo<Value>& args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int status = args[0]->Int32Value(context).ToChecked();
  exit(status);
}

void Kill(const FunctionCallbackInfo<Value>& args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int pid = args[0]->Int32Value(context).ToChecked();
  int signum = args[1]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, kill(pid, signum)));
}

//TODO: CPU Info:
/*
https://github.com/nodejs/node/blob/4438852aa16689b841e5ffbca4a24fc36a0fe33c/src/node_os.cc#L101
https://github.com/libuv/libuv/blob/c70dd705bc2adc488ddffcdc12f0c610d116e77b/src/unix/linux-core.c#L610
*/

void CPUUsage(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  Local<Float64Array> array = args[0].As<Float64Array>();
  Local<ArrayBuffer> ab = array->Buffer();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  double *fields = static_cast<double *>(backing->Data());
  fields[0] = (JUST_MICROS_PER_SEC * usage.ru_utime.tv_sec) 
    + usage.ru_utime.tv_usec;
  fields[1] = (JUST_MICROS_PER_SEC * usage.ru_stime.tv_sec) 
    + usage.ru_stime.tv_usec;
  fields[2] = usage.ru_maxrss;
  fields[3] = usage.ru_ixrss;
  fields[4] = usage.ru_idrss;
  fields[5] = usage.ru_isrss;
  fields[6] = usage.ru_minflt;
  fields[7] = usage.ru_majflt;
  fields[8] = usage.ru_nswap;
  fields[9] = usage.ru_inblock;
  fields[10] = usage.ru_oublock;
  fields[11] = usage.ru_msgsnd;
  fields[12] = usage.ru_msgrcv;
  fields[13] = usage.ru_nsignals;
  fields[14] = usage.ru_nvcsw;
  fields[15] = usage.ru_nivcsw;
}

void PID(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  args.GetReturnValue().Set(Integer::New(isolate, getpid()));
}

void Errno(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  args.GetReturnValue().Set(Integer::New(isolate, errno));
}

void StrError(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int err = args[0]->IntegerValue(context).ToChecked();
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, 
    strerror(err)).ToLocalChecked());
}

void Sleep(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int seconds = args[0]->IntegerValue(context).ToChecked();
  sleep(seconds);
}

void USleep(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int microseconds = args[0]->IntegerValue(context).ToChecked();
  usleep(microseconds);
}

void NanoSleep(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int seconds = args[0]->IntegerValue(context).ToChecked();
  int nanoseconds = args[1]->IntegerValue(context).ToChecked();
  struct timespec sleepfor;
  sleepfor.tv_sec = seconds;
  sleepfor.tv_nsec = nanoseconds;
  nanosleep(&sleepfor, NULL);
}

void MemoryUsage(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  ssize_t rss = process_memory_usage();
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);
  Local<Float64Array> array = args[0].As<Float64Array>();
  Local<ArrayBuffer> ab = array->Buffer();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  double *fields = static_cast<double *>(backing->Data());
  fields[0] = rss;
  fields[1] = v8_heap_stats.total_heap_size();
  fields[2] = v8_heap_stats.used_heap_size();
  fields[3] = v8_heap_stats.external_memory();
  fields[4] = v8_heap_stats.does_zap_garbage();
  fields[5] = v8_heap_stats.heap_size_limit();
  fields[6] = v8_heap_stats.malloced_memory();
  fields[7] = v8_heap_stats.number_of_detached_contexts();
  fields[8] = v8_heap_stats.number_of_native_contexts();
  fields[9] = v8_heap_stats.peak_malloced_memory();
  fields[10] = v8_heap_stats.total_available_size();
  fields[11] = v8_heap_stats.total_heap_size_executable();
  fields[12] = v8_heap_stats.total_physical_size();
  fields[13] = isolate->AdjustAmountOfExternalAllocatedMemory(0);
  args.GetReturnValue().Set(array);
}

void SharedMemoryUsage(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  v8::SharedMemoryStatistics sm_stats;
  v8::V8::GetSharedMemoryStatistics(&sm_stats);
  Local<Object> o = Object::New(isolate);
  o->Set(context, String::NewFromUtf8Literal(isolate, "readOnlySpaceSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, sm_stats.read_only_space_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "readOnlySpaceUsedSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, sm_stats.read_only_space_used_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, 
    "readOnlySpacePhysicalSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, sm_stats.read_only_space_physical_size())).Check();
  args.GetReturnValue().Set(o);
}

void HeapObjectStatistics(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  v8::HeapObjectStatistics obj_stats;
  size_t num_types = isolate->NumberOfTrackedHeapObjectTypes();
  Local<Object> res = Object::New(isolate);
  for (size_t i = 0; i < num_types; i++) {
    bool ok = isolate->GetHeapObjectStatisticsAtLastGC(&obj_stats, i);
    if (ok) {
      Local<Object> o = Object::New(isolate);
      o->Set(context, String::NewFromUtf8Literal(isolate, "subType", 
        NewStringType::kNormal), 
        String::NewFromUtf8(isolate, obj_stats.object_sub_type(), 
        NewStringType::kNormal).ToLocalChecked()).Check();
      o->Set(context, String::NewFromUtf8Literal(isolate, "count", 
        NewStringType::kNormal), 
        Integer::New(isolate, obj_stats.object_count())).Check();
      o->Set(context, String::NewFromUtf8Literal(isolate, "size", 
        NewStringType::kNormal), 
        Integer::New(isolate, obj_stats.object_size())).Check();
      res->Set(context, 
        String::NewFromUtf8(isolate, obj_stats.object_type(), 
        NewStringType::kNormal).ToLocalChecked(),
        o
      ).Check();
    }
  }
  args.GetReturnValue().Set(res);
}

void HeapCodeStatistics(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  v8::HeapCodeStatistics code_stats;
  isolate->GetHeapCodeAndMetadataStatistics(&code_stats);
  Local<Object> o = Object::New(isolate);
  o->Set(context, String::NewFromUtf8Literal(isolate, "codeAndMetadataSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, code_stats.code_and_metadata_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "bytecodeAndMetadataSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, code_stats.bytecode_and_metadata_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "externalScriptSourceSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, code_stats.external_script_source_size())).Check();
  args.GetReturnValue().Set(o);
}

void HeapSpaceUsage(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  HeapSpaceStatistics s;
  size_t number_of_heap_spaces = isolate->NumberOfHeapSpaces();
  Local<Array> spaces = args[0].As<Array>();
  Local<Object> o = Object::New(isolate);
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);
  Local<Object> heaps = Object::New(isolate);
  o->Set(context, String::NewFromUtf8Literal(isolate, "totalHeapSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.total_heap_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "totalPhysicalSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.total_physical_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "usedHeapSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.used_heap_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "totalAvailableSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.total_available_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "heapSizeLimit", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.heap_size_limit())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "totalHeapSizeExecutable", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.total_heap_size_executable())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "totalGlobalHandlesSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.total_global_handles_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "usedGlobalHandlesSize", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.used_global_handles_size())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "mallocedMemory", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.malloced_memory())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "externalMemory", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.external_memory())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "peakMallocedMemory", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.peak_malloced_memory())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "nativeContexts", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.number_of_native_contexts())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "detachedContexts", 
    NewStringType::kNormal), 
    Integer::New(isolate, v8_heap_stats.number_of_detached_contexts())).Check();
  o->Set(context, String::NewFromUtf8Literal(isolate, "heapSpaces", 
    NewStringType::kNormal), heaps).Check();
  for (size_t i = 0; i < number_of_heap_spaces; i++) {
    isolate->GetHeapSpaceStatistics(&s, i);
    Local<Float64Array> array = spaces->Get(context, i)
      .ToLocalChecked().As<Float64Array>();
    Local<ArrayBuffer> ab = array->Buffer();
    std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
    double *fields = static_cast<double *>(backing->Data());
    fields[0] = s.physical_space_size();
    fields[1] = s.space_available_size();
    fields[2] = s.space_size();
    fields[3] = s.space_used_size();
    heaps->Set(context, String::NewFromUtf8(isolate, s.space_name(), 
      NewStringType::kNormal).ToLocalChecked(), array).Check();
  }
  args.GetReturnValue().Set(o);
}

void FreeMemory(void* buf, size_t length, void* data) {
  //fprintf(stderr, "free: %lu\n", length);
}

void Memcpy(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();

  Local<ArrayBuffer> abdest = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> bdest = abdest->GetBackingStore();
  char *dest = static_cast<char *>(bdest->Data());

  Local<ArrayBuffer> absource = args[1].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> bsource = absource->GetBackingStore();
  char *source = static_cast<char *>(bsource->Data());
  int slen = bsource->ByteLength();

  int argc = args.Length();
  int off = 0;
  if (argc > 2) {
    off = args[2]->Int32Value(context).ToChecked();
  }
  int len = slen;
  if (argc > 3) {
    len = args[3]->Int32Value(context).ToChecked();
  }
  int off2 = 0;
  if (argc > 4) {
    off2 = args[4]->Int32Value(context).ToChecked();
  }
  if (len == 0) return;
  dest = dest + off;
  source = source + off2;
  memcpy(dest, source, len);
  args.GetReturnValue().Set(Integer::New(isolate, len));
}

void Calloc(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  uint32_t count = args[0]->Uint32Value(context).ToChecked();
  uint32_t size = 0;
  void* chunk;
  if (args[1]->IsString()) {
    Local<String> str = args[1].As<String>();
    size = str->Length();
    chunk = calloc(count, size);
    int written;
    str->WriteUtf8(isolate, (char*)chunk, size, &written, 
      String::HINT_MANY_WRITES_EXPECTED | String::NO_NULL_TERMINATION);
  } else {
    size = args[1]->Uint32Value(context).ToChecked();
    chunk = calloc(count, size);
  }
  bool shared = false;
  if (args.Length() > 2) {
    shared = args[2]->BooleanValue(isolate);
  }
  if (shared) {
    std::unique_ptr<BackingStore> backing =
        SharedArrayBuffer::NewBackingStore(chunk, count * size, 
          FreeMemory, nullptr);
    Local<SharedArrayBuffer> ab =
        SharedArrayBuffer::New(isolate, std::move(backing));
    args.GetReturnValue().Set(ab);
  } else {
    std::unique_ptr<BackingStore> backing =
        ArrayBuffer::NewBackingStore(chunk, count * size, FreeMemory, nullptr);
    Local<ArrayBuffer> ab =
        ArrayBuffer::New(isolate, std::move(backing));
    args.GetReturnValue().Set(ab);
  }
}

void ReadString(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  char *data = static_cast<char *>(backing->Data());
  int len = backing->ByteLength();
  int argc = args.Length();
  if (argc > 1) {
    len = args[1]->Int32Value(context).ToChecked();
  }
  int off = 0;
  if (argc > 2) {
    off = args[2]->Int32Value(context).ToChecked();
  }
  char* source = data + off;
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, source, 
    NewStringType::kNormal, len).ToLocalChecked());
}

void WriteString(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  String::Utf8Value str(isolate, args[1]);
  int off = 0;
  if (args.Length() > 2) {
    off = args[2]->Int32Value(context).ToChecked();
  }
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  char *data = static_cast<char *>(backing->Data());
  char* source = data + off;
  int len = str.length();
  // TODO: check overflow
  memcpy(source, *str, len);
  args.GetReturnValue().Set(Integer::New(isolate, len));
}

void Fcntl(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int flags = args[1]->Int32Value(context).ToChecked();
  if (args.Length() > 2) {
    int val = args[2]->Int32Value(context).ToChecked();
    args.GetReturnValue().Set(Integer::New(isolate, fcntl(fd, flags, val)));
    return;
  }
  args.GetReturnValue().Set(Integer::New(isolate, fcntl(fd, flags)));
}

void Cwd(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  char cwd[PATH_MAX];
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, getcwd(cwd, PATH_MAX), 
    NewStringType::kNormal).ToLocalChecked());
}

void Env(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int size = 0;
  while (environ[size]) size++;
  Local<Array> envarr = Array::New(isolate);
  for (int i = 0; i < size; ++i) {
    const char *var = environ[i];
    envarr->Set(context, i, String::NewFromUtf8(isolate, var, 
      NewStringType::kNormal, strlen(var)).ToLocalChecked()).Check();
  }
  args.GetReturnValue().Set(envarr);
}

void Timer(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int t1 = args[0]->Int32Value(context).ToChecked();
  int t2 = args[1]->Int32Value(context).ToChecked();
  int argc = args.Length();
  clockid_t cid = CLOCK_MONOTONIC;
  if (argc > 2) {
    cid = (clockid_t)args[2]->Int32Value(context).ToChecked();
  }
  int flags = TFD_NONBLOCK | TFD_CLOEXEC;
  if (argc > 3) {
    flags = args[3]->Int32Value(context).ToChecked();
  }
  int fd = timerfd_create(cid, flags);
  if (fd == -1) {
    args.GetReturnValue().Set(Integer::New(isolate, fd));
    return;
  }
  struct itimerspec ts;
  ts.it_interval.tv_sec = t1 / 1000;
	ts.it_interval.tv_nsec = t1 % 1000;
	ts.it_value.tv_sec = t2 / 1000;
	ts.it_value.tv_nsec = t2 % 1000;  
  int r = timerfd_settime(fd, 0, &ts, NULL);
  if (r == -1) {
    args.GetReturnValue().Set(Integer::New(isolate, r));
    return;
  }
  args.GetReturnValue().Set(Integer::New(isolate, fd));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> sys = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, sys, "calloc", Calloc);
  SET_METHOD(isolate, sys, "readString", ReadString);
  SET_METHOD(isolate, sys, "writeString", WriteString);
  SET_METHOD(isolate, sys, "fcntl", Fcntl);
  SET_METHOD(isolate, sys, "memcpy", Memcpy);
  SET_METHOD(isolate, sys, "sleep", Sleep);
  SET_METHOD(isolate, sys, "timer", Timer);
  SET_METHOD(isolate, sys, "memoryUsage", MemoryUsage);
  SET_METHOD(isolate, sys, "heapUsage", HeapSpaceUsage);
  SET_METHOD(isolate, sys, "sharedMemoryUsage", SharedMemoryUsage);
  SET_METHOD(isolate, sys, "heapObjectStatistics", HeapObjectStatistics);
  SET_METHOD(isolate, sys, "heapCodeStatistics", HeapCodeStatistics);
  SET_METHOD(isolate, sys, "pid", PID);
  SET_METHOD(isolate, sys, "errno", Errno);
  SET_METHOD(isolate, sys, "strerror", StrError);
  SET_METHOD(isolate, sys, "cpuUsage", CPUUsage);
  SET_METHOD(isolate, sys, "hrtime", HRTime);
  SET_METHOD(isolate, sys, "cwd", Cwd);
  SET_METHOD(isolate, sys, "env", Env);
  SET_METHOD(isolate, sys, "spawn", Spawn);
  SET_METHOD(isolate, sys, "runMicroTasks", RunMicroTasks);
  SET_METHOD(isolate, sys, "nextTick", EnqueueMicrotask);
  SET_METHOD(isolate, sys, "exit", Exit);
  SET_METHOD(isolate, sys, "kill", Kill);
  SET_METHOD(isolate, sys, "usleep", USleep);
  SET_METHOD(isolate, sys, "nanosleep", NanoSleep);
  SET_VALUE(isolate, sys, "CLOCK_MONOTONIC", Integer::New(isolate, 
    CLOCK_MONOTONIC));
  SET_VALUE(isolate, sys, "TFD_NONBLOCK", Integer::New(isolate, 
    TFD_NONBLOCK));
  SET_VALUE(isolate, sys, "TFD_CLOEXEC", Integer::New(isolate, 
    TFD_CLOEXEC));
  SET_VALUE(isolate, sys, "F_GETFL", Integer::New(isolate, F_GETFL));
  SET_VALUE(isolate, sys, "F_SETFL", Integer::New(isolate, F_SETFL));
  SET_VALUE(isolate, sys, "STDIN_FILENO", Integer::New(isolate, 
    STDIN_FILENO));
  SET_VALUE(isolate, sys, "STDOUT_FILENO", Integer::New(isolate, 
    STDOUT_FILENO));
  SET_VALUE(isolate, sys, "STDERR_FILENO", Integer::New(isolate, 
    STDERR_FILENO));
  SET_VALUE(isolate, sys, "SIGTERM", Integer::New(isolate, SIGTERM));
  SET_VALUE(isolate, sys, "SIGHUP", Integer::New(isolate, SIGHUP));
  SET_VALUE(isolate, sys, "SIGUSR1", Integer::New(isolate, SIGUSR1));
  SET_MODULE(isolate, target, "sys", sys);
}

}

namespace net {

void Socket(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int domain = args[0]->Int32Value(context).ToChecked();
  int type = args[1]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, socket(domain, type, 0)));
}

void SetSockOpt(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int level = args[1]->Int32Value(context).ToChecked();
  int option = args[2]->Int32Value(context).ToChecked();
  int value = args[3]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, setsockopt(fd, level, 
    option, &value, sizeof(int))));
}

void GetSockName(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int domain = args[1]->Int32Value(context).ToChecked();
  if (domain == AF_INET) {
    Local<Array> answer = args[2].As<Array>();
    struct sockaddr_in address;
    socklen_t namelen = (socklen_t)sizeof(address);
    getsockname(fd, (struct sockaddr*)&address, &namelen);
    char addr[INET_ADDRSTRLEN];
    socklen_t size = sizeof(address);
    inet_ntop(AF_INET, &address.sin_addr, addr, size);
    answer->Set(context, 0, String::NewFromUtf8(isolate, addr, 
      v8::NewStringType::kNormal, strlen(addr)).ToLocalChecked()).Check();
    answer->Set(context, 1, Integer::New(isolate, 
      ntohs(address.sin_port))).Check();
    args.GetReturnValue().Set(answer);
  } else {
    struct sockaddr_un address;
    socklen_t namelen = (socklen_t)sizeof(address);
    getsockname(fd, (struct sockaddr*)&address, &namelen);
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, 
      address.sun_path, v8::NewStringType::kNormal, 
      strlen(address.sun_path)).ToLocalChecked());
  }
}

void GetPeerName(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int domain = args[1]->Int32Value(context).ToChecked();
  if (domain == AF_INET) {
    Local<Array> answer = args[2].As<Array>();
    struct sockaddr_in address;
    socklen_t namelen = (socklen_t)sizeof(address);
    getpeername(fd, (struct sockaddr*)&address, &namelen);
    char addr[INET_ADDRSTRLEN];
    socklen_t size = sizeof(address);
    inet_ntop(AF_INET, &address.sin_addr, addr, size);
    answer->Set(context, 0, String::NewFromUtf8(isolate, addr, 
      v8::NewStringType::kNormal, strlen(addr)).ToLocalChecked()).Check();
    answer->Set(context, 1, Integer::New(isolate, 
      ntohs(address.sin_port))).Check();
    args.GetReturnValue().Set(answer);
  } else {
    struct sockaddr_un address;
    socklen_t namelen = (socklen_t)sizeof(address);
    getpeername(fd, (struct sockaddr*)&address, &namelen);
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, 
      address.sun_path, v8::NewStringType::kNormal, 
      strlen(address.sun_path)).ToLocalChecked());
  }
}

void Listen(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int backlog = args[1]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, listen(fd, backlog)));
}

void SocketPair(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int domain = args[0]->Int32Value(context).ToChecked();
  int type = args[1]->Int32Value(context).ToChecked();
  Local<Array> answer = args[2].As<Array>();
  int fd[2];
  int r = socketpair(domain, type, 0, fd);
  if (r == 0) {
    answer->Set(context, 0, Integer::New(isolate, fd[0])).Check();
    answer->Set(context, 1, Integer::New(isolate, fd[1])).Check();
  }
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Connect(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int r = 0;
  if (args.Length() > 2) {
    int socktype = AF_INET;
    String::Utf8Value address(isolate, args[1]);
    int port = args[2]->Int32Value(context).ToChecked();
    struct sockaddr_in server_addr;
    server_addr.sin_family = socktype;
    server_addr.sin_port = htons(port);
    inet_pton(socktype, *address, &(server_addr.sin_addr.s_addr));
    r = connect(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
  } else {
    int socktype = AF_UNIX;
    String::Utf8Value path(isolate, args[1]);
    struct sockaddr_un server_addr;
    server_addr.sun_family = socktype;
    strncpy(server_addr.sun_path, *path, sizeof(server_addr.sun_path));
    r = connect(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
  }
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Bind(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int r = 0;
  if (args.Length() > 2) {
    int socktype = AF_INET;
    String::Utf8Value address(isolate, args[1]);
    int port = args[2]->Int32Value(context).ToChecked();
    struct sockaddr_in server_addr;
    server_addr.sin_family = socktype;
    server_addr.sin_port = htons(port);
    inet_pton(socktype, *address, &(server_addr.sin_addr.s_addr));
    r = bind(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
  } else {
    int socktype = AF_UNIX;
    String::Utf8Value path(isolate, args[1]);
    struct sockaddr_un server_addr;
    server_addr.sun_family = socktype;
    strncpy(server_addr.sun_path, *path, sizeof(server_addr.sun_path));
    r = bind(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
  }
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Accept(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, accept(fd, NULL, NULL)));
}

void Read(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  Local<ArrayBuffer> buf = args[1].As<ArrayBuffer>();
  int argc = args.Length();
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  int off = 0;
  if (argc > 2) {
    off = args[2]->Int32Value(context).ToChecked();
  }
  int len = backing->ByteLength() - off;
  if (argc > 3) {
    len = args[3]->Int32Value(context).ToChecked();
  }
  const char* data = (const char*)backing->Data() + off;
  int r = read(fd, (void*)data, len);
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Recv(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  Local<ArrayBuffer> buf = args[1].As<ArrayBuffer>();
  int argc = args.Length();
  int flags = 0;
  int off = 0;
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  int len = backing->ByteLength() - off;
  if (argc > 2) {
    off = args[2]->Int32Value(context).ToChecked();
  }
  if (argc > 3) {
    len = args[3]->Int32Value(context).ToChecked();
  }
  if (argc > 4) {
    flags = args[4]->Int32Value(context).ToChecked();
  }
  const char* data = (const char*)backing->Data() + off;
  int r = recv(fd, (void*)data, len, flags);
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Write(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  Local<ArrayBuffer> ab = args[1].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  int argc = args.Length();
  int len = 0;
  if (argc > 2) {
    len = args[2]->Int32Value(context).ToChecked();
  } else {
    len = backing->ByteLength();
  }
  int off = 0;
  if (argc > 3) {
    off = args[3]->Int32Value(context).ToChecked();
  }
  char* dest = (char*)backing->Data() + off;
  args.GetReturnValue().Set(Integer::New(isolate, write(fd, 
    dest, len)));
}

void Writev(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  args.GetReturnValue().Set(Integer::New(isolate, 0));
}

void Send(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<Object> obj;
  int fd = args[0]->Int32Value(context).ToChecked();
  Local<ArrayBuffer> buf = args[1].As<ArrayBuffer>();
  int argc = args.Length();
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  int len = backing->ByteLength();
  if (argc > 2) {
    len = args[2]->Int32Value(context).ToChecked();
  }
  int flags = MSG_NOSIGNAL;
  if (argc > 3) {
    flags = args[3]->Int32Value(context).ToChecked();
  }
  int r = send(fd, backing->Data(), len, flags);
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Close(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, close(fd)));
}

void Shutdown(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int how = SHUT_RDWR;
  if (args.Length() > 1) {
    how = args[1]->Int32Value(context).ToChecked();
  }
  args.GetReturnValue().Set(Integer::New(isolate, shutdown(fd, how)));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> net = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, net, "socket", Socket);
  SET_METHOD(isolate, net, "setsockopt", SetSockOpt);
  SET_METHOD(isolate, net, "listen", Listen);
  SET_METHOD(isolate, net, "connect", Connect);
  SET_METHOD(isolate, net, "socketpair", SocketPair);
  SET_METHOD(isolate, net, "bind", Bind);
  SET_METHOD(isolate, net, "accept", Accept);
  SET_METHOD(isolate, net, "read", Read);
  SET_METHOD(isolate, net, "recv", Recv);
  SET_METHOD(isolate, net, "write", Write);
  SET_METHOD(isolate, net, "writev", Writev);
  SET_METHOD(isolate, net, "send", Send);
  SET_METHOD(isolate, net, "close", Close);
  SET_METHOD(isolate, net, "shutdown", Shutdown);
  SET_METHOD(isolate, net, "getsockname", GetSockName);
  SET_METHOD(isolate, net, "getpeername", GetPeerName);
  SET_VALUE(isolate, net, "AF_INET", Integer::New(isolate, AF_INET));
  SET_VALUE(isolate, net, "AF_UNIX", Integer::New(isolate, AF_UNIX));
  SET_VALUE(isolate, net, "SOCK_STREAM", Integer::New(isolate, SOCK_STREAM));
  SET_VALUE(isolate, net, "SOCK_DGRAM", Integer::New(isolate, SOCK_DGRAM));
  SET_VALUE(isolate, net, "SOCK_NONBLOCK", Integer::New(isolate, 
    SOCK_NONBLOCK));
  SET_VALUE(isolate, net, "SOL_SOCKET", Integer::New(isolate, SOL_SOCKET));
  SET_VALUE(isolate, net, "SO_REUSEADDR", Integer::New(isolate, SO_REUSEADDR));
  SET_VALUE(isolate, net, "SO_REUSEPORT", Integer::New(isolate, SO_REUSEPORT));
  SET_VALUE(isolate, net, "SO_INCOMING_CPU", Integer::New(isolate, 
    SO_INCOMING_CPU));
  SET_VALUE(isolate, net, "IPPROTO_TCP", Integer::New(isolate, IPPROTO_TCP));
  SET_VALUE(isolate, net, "TCP_NODELAY", Integer::New(isolate, TCP_NODELAY));
  SET_VALUE(isolate, net, "SO_KEEPALIVE", Integer::New(isolate, SO_KEEPALIVE));
  SET_VALUE(isolate, net, "SOMAXCONN", Integer::New(isolate, SOMAXCONN));
  SET_VALUE(isolate, net, "O_NONBLOCK", Integer::New(isolate, O_NONBLOCK));
  SET_VALUE(isolate, net, "EAGAIN", Integer::New(isolate, EAGAIN));
  SET_VALUE(isolate, net, "EWOULDBLOCK", Integer::New(isolate, EWOULDBLOCK));
  SET_VALUE(isolate, net, "EINTR", Integer::New(isolate, EINTR));
  SET_MODULE(isolate, target, "net", net);
}

}

namespace loop {

void EpollCtl(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int loopfd = args[0]->Int32Value(context).ToChecked();
  int action = args[1]->Int32Value(context).ToChecked();
  int fd = args[2]->Int32Value(context).ToChecked();
  int mask = args[3]->Int32Value(context).ToChecked();
  struct epoll_event event;
  event.events = mask;
  event.data.fd = fd;
  args.GetReturnValue().Set(Integer::New(isolate, epoll_ctl(loopfd, 
    action, fd, &event)));
}

void EpollCreate(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int flags = args[0]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, epoll_create1(flags)));
}

void EpollWait(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int loopfd = args[0]->Int32Value(context).ToChecked();
  Local<ArrayBuffer> buf = args[1].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  struct epoll_event* events = (struct epoll_event*)backing->Data();
  int size = backing->ByteLength() / 12;
  int timeout = -1;
  int argc = args.Length();
  if (argc > 2) {
    timeout = args[2]->Int32Value(context).ToChecked();
  }
  if (argc > 3) {
    Local<ArrayBuffer> buf = args[3].As<ArrayBuffer>();
    std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
    sigset_t* set = static_cast<sigset_t*>(backing->Data());
    int r = epoll_pwait(loopfd, events, size, timeout, set);
    args.GetReturnValue().Set(Integer::New(isolate, r));
    return;
  }
  int r = epoll_wait(loopfd, events, size, timeout);
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> loop = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, loop, "control", EpollCtl);
  SET_METHOD(isolate, loop, "create", EpollCreate);
  SET_METHOD(isolate, loop, "wait", EpollWait);
  SET_VALUE(isolate, loop, "EPOLL_CTL_ADD", Integer::New(isolate, 
    EPOLL_CTL_ADD));
  SET_VALUE(isolate, loop, "EPOLL_CTL_MOD", Integer::New(isolate, 
    EPOLL_CTL_MOD));
  SET_VALUE(isolate, loop, "EPOLL_CTL_DEL", Integer::New(isolate, 
    EPOLL_CTL_DEL));
  SET_VALUE(isolate, loop, "EPOLLET", Integer::New(isolate, EPOLLET));
  SET_VALUE(isolate, loop, "EPOLLIN", Integer::New(isolate, EPOLLIN));
  SET_VALUE(isolate, loop, "EPOLLOUT", Integer::New(isolate, EPOLLOUT));
  SET_VALUE(isolate, loop, "EPOLLERR", Integer::New(isolate, EPOLLERR));
  SET_VALUE(isolate, loop, "EPOLLHUP", Integer::New(isolate, EPOLLHUP));
  SET_VALUE(isolate, loop, "EPOLLEXCLUSIVE", Integer::New(isolate, 
    EPOLLEXCLUSIVE));
  SET_VALUE(isolate, loop, "EPOLLONESHOT", Integer::New(isolate, 
    EPOLLONESHOT));
  SET_VALUE(isolate, loop, "EPOLL_CLOEXEC", Integer::New(isolate, 
    EPOLL_CLOEXEC));
  SET_MODULE(isolate, target, "loop", loop);
}

}

namespace fs {

void ReadFile(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  String::Utf8Value fname(isolate, args[0]);
  Local<String> str = just::ReadFile(isolate, *fname);
  args.GetReturnValue().Set(str);
}

void Unlink(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  String::Utf8Value fname(isolate, args[0]);
  args.GetReturnValue().Set(Integer::New(isolate, unlink(*fname)));
}

void Open(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  String::Utf8Value fname(isolate, args[0]);
  int argc = args.Length();
  int flags = O_RDONLY;
  if (argc > 1) {
    flags = args[1]->Int32Value(context).ToChecked();
  }
  int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  if (argc > 2) {
    mode = args[2]->Int32Value(context).ToChecked();
  }
  args.GetReturnValue().Set(Integer::New(isolate, open(*fname, flags, mode)));
}

void Ioctl(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int flags = args[1]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, ioctl(fd, flags)));
}

void Fstat(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  Local<BigUint64Array> answer = args[1].As<BigUint64Array>();
  Local<ArrayBuffer> ab = answer->Buffer();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  uint64_t *fields = static_cast<uint64_t *>(backing->Data());
  struct stat s;
  int rc = fstat(fd, &s);
  if (rc == 0) {
    fields[0] = s.st_dev;
    fields[1] = s.st_mode;
    fields[2] = s.st_nlink;
    fields[3] = s.st_uid;
    fields[4] = s.st_gid;
    fields[5] = s.st_rdev;
    fields[6] = s.st_ino;
    fields[7] = s.st_size;
    fields[8] = s.st_blksize;
    fields[9] = s.st_blocks;
    //fields[10] = s.st_flags;
    //fields[11] = s.st_gen;
    fields[12] = s.st_atim.tv_sec;
    fields[13] = s.st_atim.tv_nsec;
    fields[14] = s.st_mtim.tv_sec;
    fields[15] = s.st_mtim.tv_nsec;
    fields[16] = s.st_ctim.tv_sec;
    fields[17] = s.st_ctim.tv_nsec;
    args.GetReturnValue().Set(Integer::New(isolate, 0));
  }
  args.GetReturnValue().Set(Integer::New(isolate, rc));
}

void Rmdir(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  String::Utf8Value path(isolate, args[0]);
  int rc = rmdir(*path);
  args.GetReturnValue().Set(Integer::New(isolate, rc));
}

void Mkdir(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  String::Utf8Value path(isolate, args[0]);
  int mode = S_IRWXO | S_IRWXG | S_IRWXU;
  int argc = args.Length();
  if (argc > 1) {
    mode = args[1]->Int32Value(context).ToChecked();
  }
  int rc = mkdir(*path, mode);
  args.GetReturnValue().Set(Integer::New(isolate, rc));
}

void Readdir(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  String::Utf8Value path(isolate, args[0]);
  Local<Array> answer = args[1].As<Array>();
  DIR* directory = opendir(*path);
  if (directory == NULL) {
    args.GetReturnValue().Set(Null(isolate));
    return;
  }
  dirent* entry = readdir(directory);
  if (entry == NULL) {
    args.GetReturnValue().Set(Null(isolate));
    return;
  }
  int i = 0;
  while (entry) {
    Local<Object> o = Object::New(isolate);
    o->Set(context, String::NewFromUtf8Literal(isolate, "name", 
      NewStringType::kNormal), 
      String::NewFromUtf8(isolate, entry->d_name).ToLocalChecked()).Check();
    o->Set(context, String::NewFromUtf8Literal(isolate, "type", 
      NewStringType::kNormal), 
      Integer::New(isolate, entry->d_type)).Check();
    o->Set(context, String::NewFromUtf8Literal(isolate, "ino", 
      NewStringType::kNormal), 
      Integer::New(isolate, entry->d_ino)).Check();
    o->Set(context, String::NewFromUtf8Literal(isolate, "off", 
      NewStringType::kNormal), 
      Integer::New(isolate, entry->d_off)).Check();
    o->Set(context, String::NewFromUtf8Literal(isolate, "reclen", 
      NewStringType::kNormal), 
        Integer::New(isolate, entry->d_reclen)).Check();
    answer->Set(context, i++, o).Check();
    entry = readdir(directory);
    if (i == 1023) break;
  }
  closedir(directory);
  args.GetReturnValue().Set(answer);
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> fs = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, fs, "readFile", just::fs::ReadFile);
  SET_METHOD(isolate, fs, "open", just::fs::Open);
  SET_METHOD(isolate, fs, "unlink", just::fs::Unlink);
  SET_METHOD(isolate, fs, "ioctl", just::fs::Ioctl);
  SET_METHOD(isolate, fs, "rmdir", just::fs::Rmdir);
  SET_METHOD(isolate, fs, "mkdir", just::fs::Mkdir);
  SET_METHOD(isolate, fs, "fstat", just::fs::Fstat);
  SET_METHOD(isolate, fs, "readdir", just::fs::Readdir);
  // todo: move fcntl here

  SET_VALUE(isolate, fs, "O_RDONLY", Integer::New(isolate, O_RDONLY));
  SET_VALUE(isolate, fs, "O_WRONLY", Integer::New(isolate, O_WRONLY));
  SET_VALUE(isolate, fs, "O_RDWR", Integer::New(isolate, O_RDWR));
  SET_VALUE(isolate, fs, "O_CREAT", Integer::New(isolate, O_CREAT));
  SET_VALUE(isolate, fs, "O_EXCL", Integer::New(isolate, O_EXCL));
  SET_VALUE(isolate, fs, "O_APPEND", Integer::New(isolate, O_APPEND));
  SET_VALUE(isolate, fs, "O_SYNC", Integer::New(isolate, O_SYNC));
  SET_VALUE(isolate, fs, "O_TRUNC", Integer::New(isolate, O_TRUNC));

  SET_VALUE(isolate, fs, "S_IRUSR", Integer::New(isolate, S_IRUSR));
  SET_VALUE(isolate, fs, "S_IWUSR", Integer::New(isolate, S_IWUSR));
  SET_VALUE(isolate, fs, "S_IXUSR", Integer::New(isolate, S_IXUSR));
  SET_VALUE(isolate, fs, "S_IRGRP", Integer::New(isolate, S_IRGRP));
  SET_VALUE(isolate, fs, "S_IWGRP", Integer::New(isolate, S_IWGRP));
  SET_VALUE(isolate, fs, "S_IXGRP", Integer::New(isolate, S_IXGRP));
  SET_VALUE(isolate, fs, "S_IROTH", Integer::New(isolate, S_IROTH));
  SET_VALUE(isolate, fs, "S_IWOTH", Integer::New(isolate, S_IWOTH));
  SET_VALUE(isolate, fs, "S_IXOTH", Integer::New(isolate, S_IXOTH));
  SET_VALUE(isolate, fs, "S_IRWXO", Integer::New(isolate, S_IRWXO));
  SET_VALUE(isolate, fs, "S_IRWXG", Integer::New(isolate, S_IRWXG));
  SET_VALUE(isolate, fs, "S_IRWXU", Integer::New(isolate, S_IRWXU));

  SET_VALUE(isolate, fs, "DT_BLK", Integer::New(isolate, DT_BLK));
  SET_VALUE(isolate, fs, "DT_CHR", Integer::New(isolate, DT_CHR));
  SET_VALUE(isolate, fs, "DT_DIR", Integer::New(isolate, DT_DIR));
  SET_VALUE(isolate, fs, "DT_FIFO", Integer::New(isolate, DT_FIFO));
  SET_VALUE(isolate, fs, "DT_LNK", Integer::New(isolate, DT_LNK));
  SET_VALUE(isolate, fs, "DT_REG", Integer::New(isolate, DT_REG));
  SET_VALUE(isolate, fs, "DT_SOCK", Integer::New(isolate, DT_SOCK));
  SET_VALUE(isolate, fs, "DT_UNKNOWN", Integer::New(isolate, DT_UNKNOWN));

  SET_VALUE(isolate, fs, "S_IFMT", Integer::New(isolate, S_IFMT));
  SET_VALUE(isolate, fs, "S_IFSOCK", Integer::New(isolate, S_IFSOCK));
  SET_VALUE(isolate, fs, "S_IFLNK", Integer::New(isolate, S_IFLNK));
  SET_VALUE(isolate, fs, "S_IFREG", Integer::New(isolate, S_IFREG));
  SET_VALUE(isolate, fs, "S_IFBLK", Integer::New(isolate, S_IFBLK));
  SET_VALUE(isolate, fs, "S_IFDIR", Integer::New(isolate, S_IFDIR));
  SET_VALUE(isolate, fs, "S_IFCHR", Integer::New(isolate, S_IFCHR));
  SET_VALUE(isolate, fs, "S_IFIFO", Integer::New(isolate, S_IFIFO));

  SET_MODULE(isolate, target, "fs", fs);
}

}

namespace versions {

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> versions = ObjectTemplate::New(isolate);
  SET_VALUE(isolate, versions, "just", String::NewFromUtf8Literal(isolate, 
    JUST_VERSION));
  SET_VALUE(isolate, versions, "v8", String::NewFromUtf8(isolate, 
    v8::V8::GetVersion()).ToLocalChecked());
  SET_VALUE(isolate, versions, "glibc", String::NewFromUtf8(isolate, 
    gnu_get_libc_version()).ToLocalChecked());
  Local<ObjectTemplate> kernel = ObjectTemplate::New(isolate);
  utsname kernel_rec;
  int rc = uname(&kernel_rec);
  if (rc == 0) {
    kernel->Set(String::NewFromUtf8Literal(isolate, "os", 
      NewStringType::kNormal), String::NewFromUtf8(isolate, 
      kernel_rec.sysname).ToLocalChecked());
    kernel->Set(String::NewFromUtf8Literal(isolate, "release", 
      NewStringType::kNormal), String::NewFromUtf8(isolate, 
      kernel_rec.release).ToLocalChecked());
    kernel->Set(String::NewFromUtf8Literal(isolate, "version", 
      NewStringType::kNormal), String::NewFromUtf8(isolate, 
      kernel_rec.version).ToLocalChecked());
  }
  versions->Set(String::NewFromUtf8Literal(isolate, "kernel", 
    NewStringType::kNormal), kernel);
  SET_MODULE(isolate, target, "versions", versions);
}

}

namespace tty {

void TtyName(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  Local<ArrayBuffer> out = args[1].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = out->GetBackingStore();
  int r = ttyname_r(fd, (char*)backing->Data(), backing->ByteLength());
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> tty = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, tty, "ttyName", TtyName);
  SET_VALUE(isolate, tty, "O_ACCMODE", Integer::New(isolate, O_ACCMODE));
  SET_VALUE(isolate, tty, "FIOCLEX", Integer::New(isolate, FIOCLEX));
  SET_MODULE(isolate, target, "tty", tty);
}

}

void PromiseRejectCallback(PromiseRejectMessage message) {
  Local<Promise> promise = message.GetPromise();
  Isolate* isolate = promise->GetIsolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  PromiseRejectEvent event = message.GetEvent();
  const unsigned int argc = 3;
  Local<Object> globalInstance = context->Global();
  TryCatch try_catch(isolate);
  Local<Value> func = globalInstance->Get(context, 
    String::NewFromUtf8Literal(isolate, "onUnhandledRejection", 
      NewStringType::kNormal)).ToLocalChecked();
  if (try_catch.HasCaught()) {
    fprintf(stderr, "PromiseRejectCallback: Get\n");
    //dv8::ReportException(isolate, &try_catch);
    return;
  }
  Local<Function> onUnhandledRejection = Local<Function>::Cast(func);
  if (try_catch.HasCaught()) {
    fprintf(stderr, "PromiseRejectCallback: Cast\n");
    //dv8::ReportException(isolate, &try_catch);
    return;
  }
  Local<Value> value = message.GetValue();
  if (value.IsEmpty()) value = Undefined(isolate);
  Local<Value> argv[argc] = { promise, value, Integer::New(isolate, event) };
  onUnhandledRejection->Call(context, globalInstance, 3, argv);
  if (try_catch.HasCaught()) {
    fprintf(stderr, "PromiseRejectCallback: Call\n");
    //dv8::ReportException(isolate, &try_catch);
  }
}

void InitModules(Isolate* isolate, Local<ObjectTemplate> just) {
  vm::Init(isolate, just);
  tty::Init(isolate, just);
  fs::Init(isolate, just);
  sys::Init(isolate, just);
  net::Init(isolate, just);
  loop::Init(isolate, just);
  versions::Init(isolate, just);
}

int CreateIsolate(int argc, char** argv, InitModulesCallback InitModules, 
  const char* js, unsigned int js_len, struct iovec* buf, int fd) {
  uint64_t start = hrtime();
  Isolate::CreateParams create_params;
  int statusCode = 0;
  // todo: use our own allocator for array buffers off the v8 heap
  // then we don't need sys.calloc
  create_params.array_buffer_allocator = 
    ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate *isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    isolate->SetCaptureStackTraceForUncaughtExceptions(true, 1000, 
      StackTrace::kDetailed);

    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);

    Local<ObjectTemplate> just = ObjectTemplate::New(isolate);
    SET_METHOD(isolate, just, "print", just::Print);
    SET_METHOD(isolate, just, "error", just::Error);
    SET_VALUE(isolate, just, "START", BigInt::New(isolate, start));

    InitModules(isolate, just);

    global->Set(String::NewFromUtf8Literal(isolate, "just", 
      NewStringType::kNormal), just);

    Local<Context> context = Context::New(isolate, NULL, global);
    Context::Scope context_scope(context);
    context->AllowCodeGenerationFromStrings(false);
    isolate->SetPromiseRejectCallback(PromiseRejectCallback);
    Local<Array> arguments = Array::New(isolate);
    for (int i = 0; i < argc; i++) {
      arguments->Set(context, i, String::NewFromUtf8(isolate, argv[i], 
        NewStringType::kNormal, strlen(argv[i])).ToLocalChecked()).Check();
    }
    Local<Object> globalInstance = context->Global();
    globalInstance->Set(context, String::NewFromUtf8Literal(isolate, 
      "global", 
      NewStringType::kNormal), globalInstance).Check();
    Local<Value> obj = globalInstance->Get(context, 
      String::NewFromUtf8Literal(
        isolate, "just", 
        NewStringType::kNormal)).ToLocalChecked();
    Local<Object> justInstance = Local<Object>::Cast(obj);
    if (buf != NULL) {
      std::unique_ptr<BackingStore> backing =
          SharedArrayBuffer::NewBackingStore(buf->iov_base, buf->iov_len, 
          just::sys::FreeMemory, nullptr);
      Local<SharedArrayBuffer> ab =
          SharedArrayBuffer::New(isolate, std::move(backing));
      justInstance->Set(context, String::NewFromUtf8Literal(isolate, 
        "buffer", NewStringType::kNormal), ab).Check();
    }
    if (fd != 0) {
      justInstance->Set(context, String::NewFromUtf8Literal(isolate, "fd", 
        NewStringType::kNormal), 
        Integer::New(isolate, fd)).Check();
    }
    justInstance->Set(context, String::NewFromUtf8Literal(isolate, "args", 
      NewStringType::kNormal), arguments).Check();
    if (js_len > 0) {
      justInstance->Set(context, String::NewFromUtf8Literal(isolate, 
        "workerSource", NewStringType::kNormal), 
        String::NewFromUtf8(isolate, js, NewStringType::kNormal, 
        js_len).ToLocalChecked()).Check();
    }
    TryCatch try_catch(isolate);
    ScriptOrigin baseorigin(
      String::NewFromUtf8Literal(isolate, "just.js", 
      NewStringType::kNormal), // resource name
      Integer::New(isolate, 0), // line offset
      Integer::New(isolate, 0),  // column offset
      False(isolate), // is shared cross-origin
      Local<Integer>(),  // script id
      Local<Value>(), // source map url
      False(isolate), // is opaque
      False(isolate), // is wasm
      True(isolate)  // is module
    );
    Local<Module> module;
    Local<String> base;
    builtin* main = builtins["just"];
    if (main == nullptr) {
      return 1;
    }
    base = String::NewFromUtf8(isolate, main->source, NewStringType::kNormal, 
      main->size).ToLocalChecked();
    ScriptCompiler::Source basescript(base, baseorigin);
    if (!ScriptCompiler::CompileModule(isolate, &basescript).ToLocal(&module)) {
      PrintStackTrace(isolate, try_catch);
      return 1;
    }

    Maybe<bool> ok = module->InstantiateModule(context, 
      just::OnModuleInstantiate);
    if (!ok.ToChecked()) {
      just::PrintStackTrace(isolate, try_catch);
      return 1;
    }
    MaybeLocal<Value> result = module->Evaluate(context);
		if (result.IsEmpty()) {
      if (try_catch.HasCaught()) {
        just::PrintStackTrace(isolate, try_catch);
        return 2;
      }
    }

    Local<Value> func = globalInstance->Get(context, 
      String::NewFromUtf8Literal(isolate, "onExit", 
        NewStringType::kNormal)).ToLocalChecked();
    if (func->IsFunction()) {
      Local<Function> onExit = Local<Function>::Cast(func);
      Local<Value> argv[0] = { };
      MaybeLocal<Value> result = onExit->Call(context, globalInstance, 0, argv);
      if (!result.IsEmpty()) {
        statusCode = result.ToLocalChecked()->Uint32Value(context).ToChecked();
      }
      if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
        just::PrintStackTrace(isolate, try_catch);
        return 2;
      }
      statusCode = result.ToLocalChecked()->Uint32Value(context).ToChecked();
    }
    isolate->ContextDisposedNotification();
    isolate->LowMemoryNotification();
    isolate->ClearKeptObjects();
    bool stop = false;
    while(!stop) {
      stop = isolate->IdleNotificationDeadline(1);  
    }
  }
  isolate->Dispose();
  delete create_params.array_buffer_allocator;
  isolate = nullptr;
  return statusCode;
}

int CreateIsolate(int argc, char** argv, InitModulesCallback InitModules) {
  return CreateIsolate(argc, argv, InitModules, NULL, 0, NULL, 0);
}

}
#endif
