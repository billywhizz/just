#ifndef JUST_H
#define JUST_H

#include <v8.h>
#include <picohttpparser.h>
#include <libplatform/libplatform.h>
#include <unistd.h>
#include <limits.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include <sys/resource.h> /* getrusage */
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <netinet/tcp.h>

#define JUST_MAX_HEADERS 16
#define JUST_MICROS_PER_SEC 1e6

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

typedef void    (*InitModulesCallback) (Isolate*, Local<ObjectTemplate>);

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

MaybeLocal<String> ReadFile(Isolate *isolate, const char *name) {
  FILE *file = fopen(name, "rb");
  if (file == NULL) {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, 
      "Bad File", NewStringType::kNormal).ToLocalChecked()));
    return MaybeLocal<String>();
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
      return MaybeLocal<String>();
    }
  }
  fclose(file);
  MaybeLocal<String> result = String::NewFromUtf8(isolate, chars, 
    NewStringType::kNormal, static_cast<int>(size));
  return result;
}

MaybeLocal<Module> OnModuleInstantiate(Local<Context> context, 
  Local<String> specifier, Local<Module> referrer) {
  HandleScope handle_scope(context->GetIsolate());
  return MaybeLocal<Module>();
}

void PrintStackTrace(Isolate* isolate, const TryCatch& try_catch) {
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
  Local<Array> params_buf;
  params_buf = args[2].As<Array>();
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
    False(isolate), // is shared cross-origin
    Local<Integer>(),  // script id
    Local<Value>(), // source map url
    False(isolate), // is opaque
    False(isolate), // is wasm
    False(isolate)); // is module
  Context::Scope scope(context);
  ScriptCompiler::Source basescript(source, baseorigin);
  Local<ScriptOrModule> script;
  MaybeLocal<Function> maybe_fn = ScriptCompiler::CompileFunctionInContext(
    context, &basescript, params.size(), params.data(),
    context_extensions.size(), context_extensions.data(), 
    ScriptCompiler::kNoCompileOptions,
    ScriptCompiler::NoCacheReason::kNoCacheNoReason, &script);
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
  Local<Module> module;
  ScriptCompiler::Source basescript(source, baseorigin);
  if (!ScriptCompiler::CompileModule(isolate, &basescript).ToLocal(&module)) {
    PrintStackTrace(isolate, try_catch);
    return;
  }
  Maybe<bool> ok = module->InstantiateModule(context, OnModuleInstantiate);
  if (!ok.ToChecked()) {
    if (try_catch.HasCaught()) {
      PrintStackTrace(isolate, try_catch);
    }
    return;
  }
  MaybeLocal<Value> result = module->Evaluate(context);
  if (result.IsEmpty()) {
    if (try_catch.HasCaught()) {
      PrintStackTrace(isolate, try_catch);
      return;
    }
  }
  args.GetReturnValue().Set(result.ToLocalChecked());
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
  if (!ScriptCompiler::Compile(context, &basescript).ToLocal(&script)) {
    PrintStackTrace(isolate, try_catch);
    return;
  }
  if (try_catch.HasCaught()) {
    PrintStackTrace(isolate, try_catch);
    return;
  }
  MaybeLocal<Value> result = script->Run(context);
  if (try_catch.HasCaught()) {
    PrintStackTrace(isolate, try_catch);
    return;
  }
  args.GetReturnValue().Set(result.ToLocalChecked());
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> vm = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, vm, "compile", just::vm::CompileScript);
  SET_METHOD(isolate, vm, "runModule", just::vm::RunModule);
  SET_METHOD(isolate, vm, "runScript", just::vm::RunScript);
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
  int pid = -1; // wait for any child process
  if (args.Length() > 1) {
    pid = args[0]->IntegerValue(context).ToChecked();
  }
  // WNOHANG - don't wait/block if status not available
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
  // todo: allow passing in fds
  int fds[3];
  fds[0] = args[3]->IntegerValue(context).ToChecked();
  fds[1] = args[4]->IntegerValue(context).ToChecked();
  fds[2] = args[5]->IntegerValue(context).ToChecked();
  int len = arguments->Length();
  char* argv[len + 2];
  for (int i = 0; i < len; i++) {
    String::Utf8Value val(isolate, arguments->Get(context, i).ToLocalChecked());
    argv[i + 1] = *val;
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
  MicrotasksScope::PerformCheckpoint(isolate);
  //isolate->RunMicrotasks();
}

void EnqueueMicrotask(const FunctionCallbackInfo<Value>& args) {
  Isolate *isolate = args.GetIsolate();
  isolate->EnqueueMicrotask(args[0].As<Function>());
}

void Exit(const FunctionCallbackInfo<Value>& args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int status = args[0]->Int32Value(context).ToChecked();
  exit(status);
}

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
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, strerror(err)).ToLocalChecked());
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
  o->Set(context, String::NewFromUtf8(isolate, "totalMemory", 
    NewStringType::kNormal).ToLocalChecked(), 
    Integer::New(isolate, v8_heap_stats.total_heap_size())).Check();
  o->Set(context, String::NewFromUtf8(isolate, "totalCommittedMemory", 
    NewStringType::kNormal).ToLocalChecked(), 
    Integer::New(isolate, v8_heap_stats.total_physical_size())).Check();
  o->Set(context, String::NewFromUtf8(isolate, "usedMemory", 
    NewStringType::kNormal).ToLocalChecked(), 
    Integer::New(isolate, v8_heap_stats.used_heap_size())).Check();
  o->Set(context, String::NewFromUtf8(isolate, "availableMemory", 
    NewStringType::kNormal).ToLocalChecked(), 
    Integer::New(isolate, v8_heap_stats.total_available_size())).Check();
  o->Set(context, String::NewFromUtf8(isolate, "memoryLimit", 
    NewStringType::kNormal).ToLocalChecked(), 
    Integer::New(isolate, v8_heap_stats.heap_size_limit())).Check();
  o->Set(context, String::NewFromUtf8(isolate, "heapSpaces", 
    NewStringType::kNormal).ToLocalChecked(), heaps).Check();
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
  fprintf(stderr, "free: %lu\n", length);
  //free(buf);
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
        SharedArrayBuffer::NewBackingStore(chunk, count * size, FreeMemory, nullptr);
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
  int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (fd == -1) {
    args.GetReturnValue().Set(Integer::New(isolate, fd));
    return;
  }
  int t1 = args[0]->Int32Value(context).ToChecked();
  int t2 = args[1]->Int32Value(context).ToChecked();
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

void Signal(const FunctionCallbackInfo<Value> &args) {

}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> sys = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, sys, "calloc", Calloc);
  SET_METHOD(isolate, sys, "readString", ReadString);
  SET_METHOD(isolate, sys, "writeString", WriteString);
  SET_METHOD(isolate, sys, "fcntl", Fcntl);
  SET_METHOD(isolate, sys, "sleep", Sleep);
  SET_METHOD(isolate, sys, "timer", Timer);
  SET_METHOD(isolate, sys, "signal", Signal);
  SET_METHOD(isolate, sys, "memoryUsage", MemoryUsage);
  SET_METHOD(isolate, sys, "heapUsage", HeapSpaceUsage);
  SET_METHOD(isolate, sys, "pid", PID);
  SET_METHOD(isolate, sys, "errno", Errno);
  SET_METHOD(isolate, sys, "strerror", StrError);
  SET_METHOD(isolate, sys, "cpuUsage", CPUUsage);
  SET_METHOD(isolate, sys, "hrtime", HRTime);
  SET_METHOD(isolate, sys, "cwd", Cwd);
  SET_METHOD(isolate, sys, "end", Env);
  SET_METHOD(isolate, sys, "runMicroTasks", RunMicroTasks);
  SET_METHOD(isolate, sys, "nextTick", EnqueueMicrotask);
  SET_METHOD(isolate, sys, "exit", Exit);
  SET_METHOD(isolate, sys, "usleep", USleep);
  SET_METHOD(isolate, sys, "nanosleep", NanoSleep);
  SET_VALUE(isolate, sys, "F_GETFL", Integer::New(isolate, F_GETFL));
  SET_VALUE(isolate, sys, "F_SETFL", Integer::New(isolate, F_SETFL));
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
    answer->Set(context, 0, String::NewFromUtf8(isolate, addr, v8::NewStringType::kNormal, strlen(addr)).ToLocalChecked()).Check();
    answer->Set(context, 1, Integer::New(isolate, ntohs(address.sin_port))).Check();
    args.GetReturnValue().Set(answer);
  } else {
    struct sockaddr_un address;
    socklen_t namelen = (socklen_t)sizeof(address);
    getsockname(fd, (struct sockaddr*)&address, &namelen);
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, address.sun_path, v8::NewStringType::kNormal, strlen(address.sun_path)).ToLocalChecked());
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
    answer->Set(context, 0, String::NewFromUtf8(isolate, addr, v8::NewStringType::kNormal, strlen(addr)).ToLocalChecked()).Check();
    answer->Set(context, 1, Integer::New(isolate, ntohs(address.sin_port))).Check();
    args.GetReturnValue().Set(answer);
  } else {
    struct sockaddr_un address;
    socklen_t namelen = (socklen_t)sizeof(address);
    getpeername(fd, (struct sockaddr*)&address, &namelen);
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, address.sun_path, v8::NewStringType::kNormal, strlen(address.sun_path)).ToLocalChecked());
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
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  int r = read(fd, backing->Data(), backing->ByteLength());
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
  if (argc > 2) {
    flags = args[2]->Int32Value(context).ToChecked();
  }
  std::shared_ptr<BackingStore> backing = buf->GetBackingStore();
  int r = recv(fd, backing->Data(), backing->ByteLength(), flags);
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Write(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  Local<ArrayBuffer> ab = args[1].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  int len = 0;
  if (args.Length() > 2) {
    len = args[2]->Int32Value(context).ToChecked();
  } else {
    len = backing->ByteLength();
  }
  args.GetReturnValue().Set(Integer::New(isolate, send(fd, 
    backing->Data(), len, MSG_NOSIGNAL)));
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
  SET_METHOD(isolate, net, "getsockname", GetSockName);
  SET_METHOD(isolate, net, "getpeername", GetPeerName);
  SET_VALUE(isolate, net, "AF_INET", Integer::New(isolate, AF_INET));
  SET_VALUE(isolate, net, "AF_UNIX", Integer::New(isolate, AF_UNIX));
  SET_VALUE(isolate, net, "SOCK_STREAM", Integer::New(isolate, SOCK_STREAM));
  SET_VALUE(isolate, net, "SOCK_NONBLOCK", Integer::New(isolate, SOCK_NONBLOCK));
  SET_VALUE(isolate, net, "SOL_SOCKET", Integer::New(isolate, SOL_SOCKET));
  SET_VALUE(isolate, net, "SO_REUSEADDR", Integer::New(isolate, SO_REUSEADDR));
  SET_VALUE(isolate, net, "SO_REUSEPORT", Integer::New(isolate, SO_REUSEPORT));
  SET_VALUE(isolate, net, "SO_INCOMING_CPU", Integer::New(isolate, SO_INCOMING_CPU));
  SET_VALUE(isolate, net, "IPPROTO_TCP", Integer::New(isolate, IPPROTO_TCP));
  SET_VALUE(isolate, net, "TCP_NODELAY", Integer::New(isolate, TCP_NODELAY));
  SET_VALUE(isolate, net, "SO_KEEPALIVE", Integer::New(isolate, SO_KEEPALIVE));
  SET_VALUE(isolate, net, "SOMAXCONN", Integer::New(isolate, SOMAXCONN));
  SET_VALUE(isolate, net, "O_NONBLOCK", Integer::New(isolate, O_NONBLOCK));
  SET_VALUE(isolate, net, "EAGAIN", Integer::New(isolate, EAGAIN));
  SET_MODULE(isolate, target, "net", net);
}

}

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
    "statusMessage").ToLocalChecked(), String::NewFromUtf8(isolate, state.status_message, 
    NewStringType::kNormal, state.status_message_len).ToLocalChecked()).Check();
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
  int timeout = -1;
  int argc = args.Length();
  if (argc > 2) {
    timeout = args[2]->Int32Value(context).ToChecked();
  }
  struct epoll_event* events = (struct epoll_event*)backing->Data();
  int size = backing->ByteLength() / 12;
  int r = epoll_wait(loopfd, events, size, timeout);
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> loop = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, loop, "control", EpollCtl);
  SET_METHOD(isolate, loop, "create", EpollCreate);
  SET_METHOD(isolate, loop, "wait", EpollWait);
  SET_VALUE(isolate, loop, "EPOLL_CTL_ADD", Integer::New(isolate, EPOLL_CTL_ADD));
  SET_VALUE(isolate, loop, "EPOLL_CTL_MOD", Integer::New(isolate, EPOLL_CTL_MOD));
  SET_VALUE(isolate, loop, "EPOLL_CTL_DEL", Integer::New(isolate, EPOLL_CTL_DEL));
  SET_VALUE(isolate, loop, "EPOLLET", Integer::New(isolate, EPOLLET));
  SET_VALUE(isolate, loop, "EPOLLIN", Integer::New(isolate, EPOLLIN));
  SET_VALUE(isolate, loop, "EPOLLOUT", Integer::New(isolate, EPOLLOUT));
  SET_VALUE(isolate, loop, "EPOLLERR", Integer::New(isolate, EPOLLERR));
  SET_VALUE(isolate, loop, "EPOLLHUP", Integer::New(isolate, EPOLLHUP));
  SET_VALUE(isolate, loop, "EPOLLEXCLUSIVE", Integer::New(isolate, EPOLLEXCLUSIVE));
  SET_VALUE(isolate, loop, "EPOLLONESHOT", Integer::New(isolate, EPOLLONESHOT));
  SET_VALUE(isolate, loop, "EPOLL_CLOEXEC", Integer::New(isolate, EPOLL_CLOEXEC));
  SET_MODULE(isolate, target, "loop", loop);
}

}

namespace fs {

void ReadFile(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value fname(isolate, args[0]);
  args.GetReturnValue().Set(just::ReadFile(isolate, *fname).ToLocalChecked());
}

void Unlink(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value fname(isolate, args[0]);
  args.GetReturnValue().Set(Integer::New(isolate, unlink(*fname)));
}

void Open(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
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
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int flags = args[1]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, ioctl(fd, flags)));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> fs = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, fs, "readFile", just::fs::ReadFile);
  SET_METHOD(isolate, fs, "open", just::fs::Open);
  SET_METHOD(isolate, fs, "unlink", just::fs::Unlink);
  SET_METHOD(isolate, fs, "ioctl", just::fs::Ioctl);
  SET_MODULE(isolate, target, "fs", fs);
}

}

namespace tty {

void TtyName(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
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

void InitModules(Isolate* isolate, Local<ObjectTemplate> just) {
  vm::Init(isolate, just);
  tty::Init(isolate, just);
  fs::Init(isolate, just);
  sys::Init(isolate, just);
  http::Init(isolate, just);
  net::Init(isolate, just);
  loop::Init(isolate, just);
}

int CreateIsolate(int argc, char** argv, InitModulesCallback InitModules, const char* js, unsigned int js_len, struct iovec* buf, int fd) {
  uint64_t start = hrtime();
  Isolate::CreateParams create_params;
  int statusCode = 0;
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

    global->Set(String::NewFromUtf8(isolate, "just", 
      NewStringType::kNormal).ToLocalChecked(), just);

    Local<Context> context = Context::New(isolate, NULL, global);
    Context::Scope context_scope(context);
    context->AllowCodeGenerationFromStrings(false);

    Local<Array> arguments = Array::New(isolate);
    for (int i = 0; i < argc; i++) {
      arguments->Set(context, i, String::NewFromUtf8(isolate, argv[i], 
        NewStringType::kNormal, strlen(argv[i])).ToLocalChecked()).Check();
    }

    Local<Object> globalInstance = context->Global();
    globalInstance->Set(context, String::NewFromUtf8(isolate, "global", 
      NewStringType::kNormal).ToLocalChecked(), globalInstance).Check();
    Local<Value> obj = globalInstance->Get(context, String::NewFromUtf8(
        isolate, "just", 
        NewStringType::kNormal).ToLocalChecked()).ToLocalChecked();
    Local<Object> justInstance = Local<Object>::Cast(obj);
    if (buf != NULL) {
      std::unique_ptr<BackingStore> backing =
          SharedArrayBuffer::NewBackingStore(buf->iov_base, buf->iov_len, just::sys::FreeMemory, nullptr);
      Local<SharedArrayBuffer> ab =
          SharedArrayBuffer::New(isolate, std::move(backing));
      justInstance->Set(context, String::NewFromUtf8(isolate, "buffer", 
        NewStringType::kNormal).ToLocalChecked(), ab).Check();
    }
    if (fd != 0) {
      justInstance->Set(context, String::NewFromUtf8(isolate, "fd", 
        NewStringType::kNormal).ToLocalChecked(), Integer::New(isolate, fd)).Check();
    }
    justInstance->Set(context, String::NewFromUtf8(isolate, "args", 
      NewStringType::kNormal).ToLocalChecked(), arguments).Check();

    const char* scriptName = "just.js";
    if (argc > 1) {
      scriptName = argv[1];
    }
    TryCatch try_catch(isolate);
    ScriptOrigin baseorigin(
      String::NewFromUtf8(isolate, scriptName, 
        NewStringType::kNormal).ToLocalChecked(), // resource name
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
    if (argc > 1) {
      base = ReadFile(isolate, argv[1]).ToLocalChecked();
    } else {
      base = String::NewFromUtf8(isolate, js, NewStringType::kNormal, 
        js_len).ToLocalChecked();
    }
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

    //v8::platform::PumpMessageLoop(platform, isolate);

    Local<Value> func = globalInstance->Get(context, 
      String::NewFromUtf8(isolate, "onExit", 
        NewStringType::kNormal).ToLocalChecked()).ToLocalChecked();
    if (func->IsFunction()) {
      Local<Function> onExit = Local<Function>::Cast(func);
      Local<Value> argv[0] = { };
      Local<Value> result = onExit->Call(context, globalInstance, 0, 
        argv).ToLocalChecked();
      statusCode = result->Uint32Value(context).ToChecked();
      //v8::platform::PumpMessageLoop(platform, isolate);
    }

    //const double kLongIdlePauseInSeconds = 2.0;
    isolate->ContextDisposedNotification();
    //isolate->IdleNotificationDeadline(platform->MonotonicallyIncreasingTime() 
    //  + kLongIdlePauseInSeconds);
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

}
#endif
