const { print, sys, net, loop } = just
const { strerror, errno, fcntl, calloc, F_SETFL, F_GETFL } = sys
const { EPOLLERR, EPOLLHUP, EPOLLIN, EPOLL_CTL_ADD, EPOLL_CLOEXEC, create, control, wait } = loop
const { close, read, EAGAIN, O_NONBLOCK } = net
function onEvent (fd, event) {
  const bytes = read(fd, rbuf)
  if (bytes < 0) {
    const err = errno()
    if (err !== EAGAIN) {
      just.print(`read error: ${strerror(err)} (${err})`)
      close(fd)
    }
    return
  }
  total += bytes
  if (bytes === 0 || (event & EPOLLERR || event & EPOLLHUP)) {
    close(fd)
    close(loopfd)
    print(total)
  }
}
function setFlag (fd, flag) {
  let flags = fcntl(fd, F_GETFL, 0)
  flags |= flag
  return fcntl(fd, F_SETFL, flags)
}
const BUFSIZE = 65536
const EVENTS = 1024
let total = 0
const stdin = 0
let r = 0
const rbuf = calloc(1, BUFSIZE)
const evbuf = calloc(EVENTS, 12)
const events = new Uint32Array(evbuf)
const loopfd = create(EPOLL_CLOEXEC)
setFlag(stdin, O_NONBLOCK)
r = control(loopfd, EPOLL_CTL_ADD, stdin, EPOLLIN)
r = wait(loopfd, evbuf)
while (r > 0) {
  let off = 0
  for (let i = 0; i < r; i++) {
    const fd = events[off + 1]
    onEvent(fd, events[off])
    off += 3
  }
  r = wait(loopfd, evbuf)
}
