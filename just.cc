#include <v8.h>
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
#include <unordered_map>
#include "builtins.h"

#define MICROS_PER_SEC 1e6

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

enum handleType {
  NONE,
  SOCKET,
  TIMER,
  SIGNAL
};

typedef struct handle handle;

struct handle {
  int fd;
  handleType type;
  struct iovec* in;
  struct iovec* out;
  void* data;
};

std::unordered_map<std::string, struct handle*> handles;

MaybeLocal<String> ReadFile(Isolate *isolate, const char *name) {
  FILE *file = fopen(name, "rb");
  if (file == NULL) {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Bad File", NewStringType::kNormal).ToLocalChecked()));
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
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Read Error", NewStringType::kNormal).ToLocalChecked()));
      delete[] chars;
      return MaybeLocal<String>();
    }
  }
  fclose(file);
  MaybeLocal<String> result = String::NewFromUtf8(isolate, chars, NewStringType::kNormal, static_cast<int>(size));
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

namespace sys {

void WaitPID(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  v8::HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<ArrayBuffer> ab = args[0].As<Int32Array>()->Buffer();
  int *fields = static_cast<int *>(ab->GetContents().Data());
  int pid = -1; // wait for any child process
  if (args.Length() > 1) {
    pid = args[0]->IntegerValue(context).ToChecked();
  }
  // WNOHANG - don't wait/block if status not available
  fields[1] = waitpid(pid, &fields[0], WNOHANG); 
  if (fields[1] < 0) {
    Local<Object> globalInstance = context->Global();
    globalInstance->Set(context, v8::String::NewFromUtf8(isolate, "errno", 
      v8::NewStringType::kNormal).ToLocalChecked(), Integer::New(isolate, 
      errno));
  }
  args.GetReturnValue().Set(args[0]);
}

void Spawn(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  v8::HandleScope handleScope(isolate);
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
  v8::HandleScope handleScope(isolate);
  Local<ArrayBuffer> ab = args[0].As<BigUint64Array>()->Buffer();
  uint64_t *fields = static_cast<uint64_t *>(ab->GetContents().Data());
  fields[0] = just_hrtime();
}

void RunMicroTasks(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  v8::MicrotasksScope::PerformCheckpoint(isolate);
  //isolate->RunMicrotasks();
}

void EnqueueMicrotask(const FunctionCallbackInfo<Value>& args) {
  Isolate *isolate = args.GetIsolate();
  isolate->EnqueueMicrotask(args[0].As<Function>());
}

void Exit(const FunctionCallbackInfo<Value>& args) {
  Isolate *isolate = args.GetIsolate();
  v8::HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int status = args[0]->Int32Value(context).ToChecked();
  exit(status);
}

void CPUUsage(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  v8::HandleScope handleScope(isolate);
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  Local<Float64Array> array = args[0].As<Float64Array>();
  Local<ArrayBuffer> ab = array->Buffer();
  double *fields = static_cast<double *>(ab->GetContents().Data());
  fields[0] = (MICROS_PER_SEC * usage.ru_utime.tv_sec) 
    + usage.ru_utime.tv_usec;
  fields[1] = (MICROS_PER_SEC * usage.ru_stime.tv_sec) 
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
  v8::HandleScope handleScope(isolate);
  args.GetReturnValue().Set(Integer::New(isolate, getpid()));
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
  v8::HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int microseconds = args[0]->IntegerValue(context).ToChecked();
  usleep(microseconds);
}

void NanoSleep(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  v8::HandleScope handleScope(isolate);
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
  v8::HandleScope handleScope(isolate);
  ssize_t rss = just_process_memory_usage();
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);
  Local<Float64Array> array = args[0].As<Float64Array>();
  Local<ArrayBuffer> ab = array->Buffer();
  double *fields = static_cast<double *>(ab->GetContents().Data());
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
  v8::HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  HeapSpaceStatistics s;
  size_t number_of_heap_spaces = isolate->NumberOfHeapSpaces();
  Local<Array> spaces = args[0].As<Array>();
  Local<Object> o = Object::New(isolate);
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);
  Local<Object> heaps = Object::New(isolate);
  o->Set(context, String::NewFromUtf8(isolate, "totalMemory", 
    v8::NewStringType::kNormal).ToLocalChecked(), 
    Integer::New(isolate, v8_heap_stats.total_heap_size()));
  o->Set(context, String::NewFromUtf8(isolate, "totalCommittedMemory", 
    v8::NewStringType::kNormal).ToLocalChecked(), 
    Integer::New(isolate, v8_heap_stats.total_physical_size()));
  o->Set(context, String::NewFromUtf8(isolate, "usedMemory", 
    v8::NewStringType::kNormal).ToLocalChecked(), 
    Integer::New(isolate, v8_heap_stats.used_heap_size()));
  o->Set(context, String::NewFromUtf8(isolate, "availableMemory", 
    v8::NewStringType::kNormal).ToLocalChecked(), 
    Integer::New(isolate, v8_heap_stats.total_available_size()));
  o->Set(context, String::NewFromUtf8(isolate, "memoryLimit", 
    v8::NewStringType::kNormal).ToLocalChecked(), 
    Integer::New(isolate, v8_heap_stats.heap_size_limit()));
  o->Set(context, String::NewFromUtf8(isolate, "heapSpaces", 
    v8::NewStringType::kNormal).ToLocalChecked(), heaps);
  for (size_t i = 0; i < number_of_heap_spaces; i++) {
    isolate->GetHeapSpaceStatistics(&s, i);
    Local<Float64Array> array = spaces->Get(context, i)
      .ToLocalChecked().As<Float64Array>();
    Local<ArrayBuffer> ab = array->Buffer();
    double *fields = static_cast<double *>(ab->GetContents().Data());
    fields[0] = s.physical_space_size();
    fields[1] = s.space_available_size();
    fields[2] = s.space_size();
    fields[3] = s.space_used_size();
    heaps->Set(context, String::NewFromUtf8(isolate, s.space_name(), 
      v8::NewStringType::kNormal).ToLocalChecked(), array);
  }
  args.GetReturnValue().Set(o);
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
      v8::String::HINT_MANY_WRITES_EXPECTED | v8::String::NO_NULL_TERMINATION);
  } else {
    size = args[1]->Uint32Value(context).ToChecked();
    chunk = calloc(count, size);
  }
  struct iovec* buf = (struct iovec*)calloc(count, sizeof(struct iovec));
  buf->iov_base = chunk;
  buf->iov_len = count * size;
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, chunk, count * size, 
    ArrayBufferCreationMode::kExternalized);
  ab->SetAlignedPointerInInternalField(0, buf);
  args.GetReturnValue().Set(ab);
}

void Free(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  struct iovec* buf = (struct iovec*)ab->GetAlignedPointerFromInternalField(0);
  free(buf->iov_base);
  isolate->AdjustAmountOfExternalAllocatedMemory(buf->iov_len * -1);
  free(buf);
  ab->SetAlignedPointerInInternalField(0, NULL);
  args.GetReturnValue().Set(Integer::New(isolate, 0));
}

void Fcntl(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  int flags = args[1]->Int32Value(context).ToChecked();
  int val = args[2]->Int32Value(context).ToChecked();
  args.GetReturnValue().Set(Integer::New(isolate, fcntl(fd, flags, val)));
}

void Cwd(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  char* cwd = (char*)calloc(1, PATH_MAX);
  size_t size = 0;
  if (getcwd(cwd, size) == NULL) {
    free(cwd);
    args.GetReturnValue().Set(v8::Null(isolate));
    return;
  }
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, cwd, 
    NewStringType::kNormal).ToLocalChecked());
  free(cwd);
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
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();

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
  if (r < 0) {
    context->Global()->Set(context, String::NewFromUtf8(isolate, "errno", 
      NewStringType::kNormal).ToLocalChecked(), Integer::New(isolate, errno));
  }
  args.GetReturnValue().Set(Integer::New(isolate, 0));
}

void Handle(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  just::handle* h = (just::handle*)calloc(1, sizeof(just::handle));
  h->fd = args[0]->Int32Value(context).ToChecked();
  int argc = args.Length();
  if (argc > 1) {
    Local<ArrayBuffer> in = args[1].As<ArrayBuffer>();
    h->in = (struct iovec*)in->GetAlignedPointerFromInternalField(0);
  }
  if (argc > 2) {
    Local<ArrayBuffer> out = args[2].As<ArrayBuffer>();
    h->out = (struct iovec*)out->GetAlignedPointerFromInternalField(0);
  }
  Local<ArrayBuffer> handle = ArrayBuffer::New(isolate, NULL, 0, 
    ArrayBufferCreationMode::kExternalized);
  handle->SetAlignedPointerInInternalField(0, h);
  handle->Set(context, String::NewFromUtf8(isolate, "fd", 
    NewStringType::kNormal).ToLocalChecked(), Integer::New(isolate, h->fd));
  args.GetReturnValue().Set(handle);
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
  Local<Object> obj;
  bool ok = args[0]->ToObject(context).ToLocal(&obj);
  just::handle* h = (just::handle*)obj->GetAlignedPointerFromInternalField(0);
  int r = read(h->fd, h->in->iov_base, h->in->iov_len);
  if (r < 0) {
    context->Global()->Set(context, String::NewFromUtf8(isolate, "errno", 
      NewStringType::kNormal).ToLocalChecked(), Integer::New(isolate, errno));
  }
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Recv(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<Object> obj;
  bool ok = args[0]->ToObject(context).ToLocal(&obj);
  just::handle* h = (just::handle*)obj->GetAlignedPointerFromInternalField(0);
  int r = recv(h->fd, h->in->iov_base, h->in->iov_len, 0);
  if (r < 0) {
    context->Global()->Set(context, String::NewFromUtf8(isolate, "errno", 
      NewStringType::kNormal).ToLocalChecked(), Integer::New(isolate, errno));
  }
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

void Write(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int fd = args[0]->Int32Value(context).ToChecked();
  Local<ArrayBuffer> ab = args[1].As<ArrayBuffer>();
  int len = 0;
  if (args.Length() > 2) {
    len = args[2]->Int32Value(context).ToChecked();
  } else {
    len = ab->GetContents().ByteLength();
  }
  args.GetReturnValue().Set(Integer::New(isolate, send(fd, 
    ab->GetContents().Data(), len, MSG_NOSIGNAL)));
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
  bool ok = args[0]->ToObject(context).ToLocal(&obj);
  just::handle* h = (just::handle*)obj->GetAlignedPointerFromInternalField(0);
  int len = 0;
  if (args.Length() > 1) {
    len = args[1]->Int32Value(context).ToChecked();
  } else {
    len = h->out->iov_len;
  }
  args.GetReturnValue().Set(Integer::New(isolate, send(h->fd, 
    h->out->iov_base, len, MSG_NOSIGNAL)));
}

void Close(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<Object> obj;
  bool ok = args[0]->ToObject(context).ToLocal(&obj);
  just::handle* h = (just::handle*)obj->GetAlignedPointerFromInternalField(0);
  args.GetReturnValue().Set(Integer::New(isolate, close(h->fd)));
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
  struct epoll_event* e = NULL;
  if (args.Length() > 3) {
    // todo: figure out how to retain a refenece to this epoll_event and 
    // free it when we close the descriptor
    int event = args[3]->Int32Value(context).ToChecked();
    e = (struct epoll_event *)calloc(1, sizeof(struct epoll_event));
    e->events = event;
    e->data.fd = fd;
  }
  args.GetReturnValue().Set(Integer::New(isolate, epoll_ctl(loopfd, 
    action, fd, e)));
}

void EpollCreate(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int flags = args[0]->Int32Value(context).ToChecked();
  int size = args[1]->Int32Value(context).ToChecked();
  Local<ArrayBuffer> ab = args[2].As<ArrayBuffer>();
  struct epoll_event* events = (struct epoll_event*)calloc(size, 
    sizeof(struct epoll_event));
  ab->SetAlignedPointerInInternalField(1, events);
  args.GetReturnValue().Set(Integer::New(isolate, epoll_create1(flags)));
}

void EpollWait(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  HandleScope handleScope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  int loopfd = args[0]->Int32Value(context).ToChecked();
  int size = args[1]->Int32Value(context).ToChecked();
  int timeout = args[2]->Int32Value(context).ToChecked();
  Local<ArrayBuffer> ab = args[3].As<ArrayBuffer>();
  struct epoll_event* events = 
    (struct epoll_event*)ab->GetAlignedPointerFromInternalField(1);
  uint32_t* fields = static_cast<uint32_t*>(ab->GetContents().Data());
  int r = epoll_wait(loopfd, events, size, timeout);
  for (int i = 0; i < r; i++) {
    fields[i * 2] = events[i].data.fd;
    fields[(i * 2) + 1] = events[i].events;
  }
  args.GetReturnValue().Set(Integer::New(isolate, r));
}

}

int CreateIsolate(v8::Platform* platform, int argc, char** argv) {
  Isolate::CreateParams create_params;
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
    just->Set(String::NewFromUtf8(isolate, "print", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::Print));

    Local<ObjectTemplate> sys = ObjectTemplate::New(isolate);
    sys->Set(String::NewFromUtf8(isolate, "calloc", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Calloc));
    sys->Set(String::NewFromUtf8(isolate, "free", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Free));
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
    sys->Set(String::NewFromUtf8(isolate, "cpuUsage", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::CPUUsage));
    sys->Set(String::NewFromUtf8(isolate, "hrtime", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::HRTime));
    sys->Set(String::NewFromUtf8(isolate, "cwd", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::sys::Cwd));
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
    net->Set(String::NewFromUtf8(isolate, "handle", 
      NewStringType::kNormal).ToLocalChecked(), 
      FunctionTemplate::New(isolate, just::net::Handle));
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
        NewStringType::kNormal, strlen(argv[i])).ToLocalChecked());
    }
    Local<Object> globalInstance = context->Global();
    globalInstance->Set(context, String::NewFromUtf8(isolate, "global", 
      NewStringType::kNormal).ToLocalChecked(), globalInstance);
    Local<Value> obj = globalInstance->Get(context, String::NewFromUtf8(
        isolate, "just", 
        NewStringType::kNormal).ToLocalChecked()).ToLocalChecked();
    Local<Object> justInstance = Local<Object>::Cast(obj);
    justInstance->Set(context, String::NewFromUtf8(isolate, "args", 
      NewStringType::kNormal).ToLocalChecked(), arguments);

    char* scriptName = "just.js";
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
      TryCatch try_catch(isolate);
      onExit->Call(context, globalInstance, 0, argv);
      if (try_catch.HasCaught()) {
        just::PrintStackTrace(isolate, try_catch);
      }
      v8::platform::PumpMessageLoop(platform, isolate);
    }
    const double kLongIdlePauseInSeconds = 1.0;
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
  return 0;
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
