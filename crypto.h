#include "just.h"

namespace just {

namespace crypto {

void RecvMsg(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  args.GetReturnValue().Set(BigInt::New(isolate, 0));
  int fd = args[0]->Uint32Value(context).ToChecked();
  Local<ArrayBuffer> ab = args[1].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  Local<Array> answer = args[2].As<Array>();
  struct iovec buf;
  buf.iov_base = backing->Data();
  buf.iov_len = backing->ByteLength();
  char ip[INET_ADDRSTRLEN];
  int iplen = sizeof ip;
  struct sockaddr_storage peer;
  struct msghdr h;
  memset(&h, 0, sizeof(h));
  memset(&peer, 0, sizeof(peer));
  h.msg_name = &peer;
  h.msg_namelen = sizeof(peer);
  h.msg_iov = &buf;
  h.msg_iovlen = 1;
  const sockaddr_in *a4 = reinterpret_cast<const sockaddr_in *>(&peer);
  int bytes = recvmsg(fd, &h, 0);
  if (bytes <= 0) {
    args.GetReturnValue().Set(BigInt::New(isolate, bytes));
    return;
  }
  inet_ntop(AF_INET, &a4->sin_addr, ip, iplen);
  answer->Set(context, 0, String::NewFromUtf8(isolate, ip, 
    v8::NewStringType::kNormal, strlen(ip)).ToLocalChecked()).Check();
  answer->Set(context, 1, Integer::New(isolate, ntohs(a4->sin_port))).Check();
  args.GetReturnValue().Set(Integer::New(isolate, bytes));
}

void Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> module = ObjectTemplate::New(isolate);
  SET_METHOD(isolate, module, "recvmsg", RecvMsg);
  SET_MODULE(isolate, target, "crypto", module);
}

}

}
