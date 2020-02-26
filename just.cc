#include <v8.h>
#include <picohttpparser.h>
#include <libplatform/libplatform.h>
#include <unistd.h>
#include <limits.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include <sys/resource.h> /* getrusage */
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <unordered_map>
#include "builtins.h"

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

ssize_t just_process_memory_usage() {
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

inline uint64_t just_hrtime() {
  struct timespec t;
  clock_t clock_id = CLOCK_MONOTONIC;
  if (clock_gettime(clock_id, &t))
    return 0;
  return t.tv_sec * (uint64_t) 1e9 + t.tv_nsec;
}

inline void JUST_SET_METHOD(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, FunctionCallback callback) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kNormal).ToLocalChecked(), 
    FunctionTemplate::New(isolate, callback));
}

inline void JUST_SET_MODULE(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, Local<ObjectTemplate> module) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kNormal).ToLocalChecked(), 
    module);
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
  char *chars = new char[size + 1];
  chars[size] = '\0';
  for (size_t i = 0; i < size;) {
    i += fread(&chars[i], 1, size - i, file);
    if (ferror(file)) {
      fclose(file);
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, 
        "Read Error", NewStringType::kNormal).ToLocalChecked()));
      delete[] chars;
      return MaybeLocal<String>();
    }
  }
  fclose(file);
  MaybeLocal<String> result = String::NewFromUtf8(isolate, chars, 
    NewStringType::kNormal, static_cast<int>(size));
  delete[] chars;
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
  Local<ArrayBuffer> ab = args[0].As<BigUint64Array>()->Buffer();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  uint64_t *fields = static_cast<uint64_t *>(backing->Data());
  fields[0] = just_hrtime();
  args.GetReturnValue().Set(ab);
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
  ssize_t rss = just_process_memory_usage();
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
  //Isolate *isolate = Isolate::GetCurrent();
  //free(buf);
  //isolate->AdjustAmountOfExternalAllocatedMemory(length * -1);
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
  std::unique_ptr<BackingStore> backing =
      ArrayBuffer::NewBackingStore(chunk, count * size, FreeMemory, nullptr);
  Local<ArrayBuffer> ab =
      ArrayBuffer::New(isolate, std::move(backing));
  //isolate->AdjustAmountOfExternalAllocatedMemory(count * size);
  args.GetReturnValue().Set(ab);
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

void Listen(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int backlog = args[1]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, listen(fd, backlog)));
}

void Bind(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  String::Utf8Value address(isolate, args[1]);
  int port = args[2]->Int32Value(context).ToChecked();
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  inet_pton(AF_INET, *address, &(server_addr.sin_addr.s_addr));
  int r = bind(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
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

}

namespace http {

typedef struct requestState requestState;

struct requestState {
  int minor_version;
  size_t method_len;
  size_t path_len;
  uint32_t body_length;
  uint32_t body_bytes;
  uint16_t header_size;
  size_t num_headers;
  struct phr_header headers[JUST_MAX_HEADERS];
  const char* path;
  const char* method;
};

requestState state;

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

}

namespace fs {

void ReadFile(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value fname(isolate, args[0]);
  args.GetReturnValue().Set(just::ReadFile(isolate, *fname).ToLocalChecked());
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

}

int CreateIsolate(Platform* platform, int argc, char** argv) {
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
    JUST_SET_METHOD(isolate, just, "print", just::Print);
    JUST_SET_METHOD(isolate, just, "error", just::Error);

    Local<ObjectTemplate> vm = ObjectTemplate::New(isolate);
    JUST_SET_METHOD(isolate, vm, "compile", just::vm::CompileScript);
    JUST_SET_METHOD(isolate, vm, "runModule", just::vm::RunModule);
    JUST_SET_METHOD(isolate, vm, "runScript", just::vm::RunScript);
    JUST_SET_MODULE(isolate, just, "vm", vm);

    Local<ObjectTemplate> tty = ObjectTemplate::New(isolate);
    tty->Set(String::NewFromUtf8(isolate, "ttyName", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::tty::TtyName));
    tty->Set(String::NewFromUtf8(isolate, "O_ACCMODE", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, O_ACCMODE));
    tty->Set(String::NewFromUtf8(isolate, "FIOCLEX", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, FIOCLEX));
    just->Set(String::NewFromUtf8(isolate, "tty", 
      NewStringType::kNormal).ToLocalChecked(), tty);

    Local<ObjectTemplate> fs = ObjectTemplate::New(isolate);
    fs->Set(String::NewFromUtf8(isolate, "readFile", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::fs::ReadFile));
    fs->Set(String::NewFromUtf8(isolate, "open", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::fs::Open));
    fs->Set(String::NewFromUtf8(isolate, "ioctl", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::fs::Ioctl));
    just->Set(String::NewFromUtf8(isolate, "fs", 
      NewStringType::kNormal).ToLocalChecked(), fs);

    Local<ObjectTemplate> sys = ObjectTemplate::New(isolate);
    sys->Set(String::NewFromUtf8(isolate, "calloc", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Calloc));
    sys->Set(String::NewFromUtf8(isolate, "readString", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::ReadString));
    sys->Set(String::NewFromUtf8(isolate, "writeString", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::WriteString));
    sys->Set(String::NewFromUtf8(isolate, "fcntl", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Fcntl));
    sys->Set(String::NewFromUtf8(isolate, "sleep", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Sleep));
    sys->Set(String::NewFromUtf8(isolate, "timer", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Timer));
    sys->Set(String::NewFromUtf8(isolate, "signal", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Signal));
    sys->Set(String::NewFromUtf8(isolate, "memoryUsage", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::MemoryUsage));
    sys->Set(String::NewFromUtf8(isolate, "heapUsage", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::HeapSpaceUsage));
    sys->Set(String::NewFromUtf8(isolate, "pid", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::PID));
    sys->Set(String::NewFromUtf8(isolate, "errno", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Errno));
    sys->Set(String::NewFromUtf8(isolate, "strerror", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::StrError));
    sys->Set(String::NewFromUtf8(isolate, "cpuUsage", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::CPUUsage));
    sys->Set(String::NewFromUtf8(isolate, "hrtime", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::HRTime));
    sys->Set(String::NewFromUtf8(isolate, "cwd", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Cwd));
    sys->Set(String::NewFromUtf8(isolate, "env", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Env));
    sys->Set(String::NewFromUtf8(isolate, "runMicroTasks", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::RunMicroTasks));
    sys->Set(String::NewFromUtf8(isolate, "nextTick", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::EnqueueMicrotask));
    sys->Set(String::NewFromUtf8(isolate, "exit", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Exit));
    sys->Set(String::NewFromUtf8(isolate, "usleep", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::USleep));
    sys->Set(String::NewFromUtf8(isolate, "nanosleep", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::NanoSleep));
    sys->Set(String::NewFromUtf8(isolate, "F_SETFL", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, F_SETFL));
    sys->Set(String::NewFromUtf8(isolate, "F_GETFL", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, F_GETFL));
    just->Set(String::NewFromUtf8(isolate, "sys", 
      NewStringType::kNormal).ToLocalChecked(), sys);

    Local<ObjectTemplate> http = ObjectTemplate::New(isolate);
    http->Set(String::NewFromUtf8(isolate, "parseRequest", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::http::ParseRequest));
    http->Set(String::NewFromUtf8(isolate, "getUrl", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::http::GetUrl));
    http->Set(String::NewFromUtf8(isolate, "getMethod", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::http::GetMethod));
    http->Set(String::NewFromUtf8(isolate, "getHeaders", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::http::GetHeaders));
    http->Set(String::NewFromUtf8(isolate, "getRequest", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::http::GetRequest));
    just->Set(String::NewFromUtf8(isolate, "http",
      NewStringType::kNormal).ToLocalChecked(), 
      http);

    Local<ObjectTemplate> net = ObjectTemplate::New(isolate);
    net->Set(String::NewFromUtf8(isolate, "socket", 
      NewStringType::kNormal).ToLocalChecked(),
      FunctionTemplate::New(isolate, just::net::Socket));
    net->Set(String::NewFromUtf8(isolate, "setsockopt", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::net::SetSockOpt));
    net->Set(String::NewFromUtf8(isolate, "listen", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::net::Listen));
    net->Set(String::NewFromUtf8(isolate, "bind", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::net::Bind));
    net->Set(String::NewFromUtf8(isolate, "accept", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::net::Accept));
    net->Set(String::NewFromUtf8(isolate, "read", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::net::Read));
    net->Set(String::NewFromUtf8(isolate, "recv", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::net::Recv));
    net->Set(String::NewFromUtf8(isolate, "write", 
      NewStringType::kNormal).ToLocalChecked(), 
        FunctionTemplate::New(isolate, just::net::Write));
    net->Set(String::NewFromUtf8(isolate, "writev", 
      NewStringType::kNormal).ToLocalChecked(), 
        FunctionTemplate::New(isolate, just::net::Writev));
    net->Set(String::NewFromUtf8(isolate, "send", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::net::Send));
    net->Set(String::NewFromUtf8(isolate, "close", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::net::Close));
    net->Set(String::NewFromUtf8(isolate, "AF_INET", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, AF_INET));
    net->Set(String::NewFromUtf8(isolate, "SOCK_STREAM", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, SOCK_STREAM));
    net->Set(String::NewFromUtf8(isolate, "SOCK_NONBLOCK", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, SOCK_NONBLOCK));
    net->Set(String::NewFromUtf8(isolate, "SOL_SOCKET", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, SOL_SOCKET));
    net->Set(String::NewFromUtf8(isolate, "SO_REUSEADDR", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, SO_REUSEADDR));
    net->Set(String::NewFromUtf8(isolate, "SO_REUSEPORT", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, SO_REUSEPORT));
    net->Set(String::NewFromUtf8(isolate, "SOMAXCONN", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, SOMAXCONN));
    net->Set(String::NewFromUtf8(isolate, "O_NONBLOCK", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, O_NONBLOCK));
    net->Set(String::NewFromUtf8(isolate, "EAGAIN", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, EAGAIN));
    just->Set(String::NewFromUtf8(isolate, "net", 
      NewStringType::kNormal).ToLocalChecked(), 
      net);

    Local<ObjectTemplate> loop = ObjectTemplate::New(isolate);
    loop->Set(String::NewFromUtf8(isolate, "control", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::loop::EpollCtl));
    loop->Set(String::NewFromUtf8(isolate, "create", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::loop::EpollCreate));
    loop->Set(String::NewFromUtf8(isolate, "wait", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::loop::EpollWait));
    loop->Set(String::NewFromUtf8(isolate, "EPOLL_CLOEXEC", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, EPOLL_CLOEXEC));
    loop->Set(String::NewFromUtf8(isolate, "EPOLL_CTL_ADD", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, EPOLL_CTL_ADD));
    loop->Set(String::NewFromUtf8(isolate, "EPOLLIN", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, EPOLLIN));
    loop->Set(String::NewFromUtf8(isolate, "EPOLLERR", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, EPOLLERR));
    loop->Set(String::NewFromUtf8(isolate, "EPOLLHUP", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, EPOLLHUP));
    loop->Set(String::NewFromUtf8(isolate, "EPOLLOUT", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, EPOLLOUT));
    loop->Set(String::NewFromUtf8(isolate, "EPOLL_CTL_DEL", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, EPOLL_CTL_DEL));
    loop->Set(String::NewFromUtf8(isolate, "EPOLLET", 
      NewStringType::kNormal).ToLocalChecked(), 
      Integer::New(isolate, EPOLLET));
    just->Set(String::NewFromUtf8(isolate, "loop", 
      NewStringType::kNormal).ToLocalChecked(), 
      loop);

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
      base = String::NewFromUtf8(isolate, just_js, NewStringType::kNormal, 
        just_js_len).ToLocalChecked();
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
    v8::platform::PumpMessageLoop(platform, isolate);
    Local<Value> func = globalInstance->Get(context, 
      String::NewFromUtf8(isolate, "onExit", 
        NewStringType::kNormal).ToLocalChecked()).ToLocalChecked();
    if (func->IsFunction()) {
      Local<Function> onExit = Local<Function>::Cast(func);
      Local<Value> argv[0] = { };
      Local<Value> result = onExit->Call(context, globalInstance, 0, 
        argv).ToLocalChecked();
      statusCode = result->Uint32Value(context).ToChecked();
      v8::platform::PumpMessageLoop(platform, isolate);
    }
    const double kLongIdlePauseInSeconds = 2.0;
    isolate->ContextDisposedNotification();
    isolate->IdleNotificationDeadline(platform->MonotonicallyIncreasingTime() 
      + kLongIdlePauseInSeconds);
    isolate->LowMemoryNotification();

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

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  signal(SIGPIPE, SIG_IGN);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);

  just::CreateIsolate(platform.get(), argc, argv);

  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  platform.reset();
  return 0;
}
