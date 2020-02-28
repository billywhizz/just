const { net, sys } = just
const { socketpair, AF_UNIX, SOCK_STREAM } = net

function threadMain () {
  const { net, sys } = just
  const fd = just.fd
  const shared = just.buffer
  const buf = sys.calloc(1, 4096)
  const u8 = new Uint8Array(shared)
  let bytes = net.recv(fd, buf)
  while (bytes > 0) {
    just.print(sys.readString(buf, bytes))
    just.print(Atomics.load(u8, 0))
    bytes = net.recv(fd, buf)
  }
  net.close(fd)
}

const fds = []
socketpair(AF_UNIX, SOCK_STREAM, fds)
const shared = sys.calloc(1, 1024, true)
const u8 = new Uint8Array(shared)
const buf = sys.calloc(1, 4096)
let source = threadMain.toString()
source = source.slice(source.indexOf('{') + 1, source.lastIndexOf('}')).trim()
const tid = just.thread.spawn(source, shared, fds[1])
let counter = 10
do {
  const len = sys.writeString(buf, `counter: ${counter}`)
  Atomics.store(u8, 0, counter)
  net.send(fds[0], buf, len)
  sys.sleep(1)
} while (--counter)
net.close(fds[0])
just.thread.join(tid)
