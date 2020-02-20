const { sys, net, loop } = just
const { F_SETFL, F_GETFL } = sys
const {
  AF_INET,
  SOCK_STREAM,
  SOCK_NONBLOCK,
  SOL_SOCKET,
  SO_REUSEADDR,
  SO_REUSEPORT,
  SOMAXCONN,
  O_NONBLOCK,
  EAGAIN
} = net
const {
  EPOLL_CLOEXEC,
  EPOLL_CTL_ADD,
  EPOLLIN,
  EPOLLERR,
  EPOLLHUP,
  EPOLLOUT,
  EPOLL_CTL_DEL
} = loop

const BUFSIZE = 524288
const EVENTS = 1024
let r = 0
let rps = 0
const loops = {}
const sockets = {}
const timers = {}
const mem = new Float64Array(16)
global.errno = 0

function onEvent (index) {
  const fd = events[index]
  const event = events[index + 1]
  if (event & EPOLLERR || event & EPOLLHUP) {
    loop.control(loopfd, EPOLL_CTL_DEL, fd)
    net.close(sockets[fd])
    delete sockets[fd]
    return
  }
  if (fd === sockfd) {
    const client = net.handle(net.accept(sockfd), rbuf, wbuf)
    let flags = sys.fcntl(client.fd, F_GETFL, 0)
    flags |= O_NONBLOCK
    sys.fcntl(client.fd, F_SETFL, flags)
    loop.control(loopfd, EPOLL_CTL_ADD, client.fd, EPOLLIN)
    sockets[client.fd] = client
    return
  }
  if (fd === timerfd) {
    const t = Object.keys(timers).length
    const s = Object.keys(sockets).length
    const rss = sys.memoryUsage(mem)[0]
    just.print(`rps ${rps} mem: ${rss} timer: ${t} socket: ${s}`)
    net.read(timers[fd])
    rps = 0
    return
  }
  if (event & EPOLLOUT) {
    return
  }
  if (event & EPOLLIN) {
    const bytes = net.recv(sockets[fd])
    if (bytes === -1 && global.errno === EAGAIN) return
    if (bytes === 0) {
      loop.control(loopfd, EPOLL_CTL_DEL, fd)
      net.close(sockets[fd])
      delete sockets[fd]
      return
    }
    net.send(sockets[fd])
    rps++
  }
}

const rbuf = sys.calloc(BUFSIZE, 1)
const wbuf = sys.calloc(1, 'HTTP/1.1 200 OK\r\nContent-Length: 51\r\n\r\n<html><body><h1>Why hello there!</h1></body></html>')
const evbuf = sys.calloc(2 * EVENTS, 4)
const tbuf = sys.calloc(1, 8) // 64 bit number
const events = new Uint32Array(evbuf)

const loopfd = loop.create(EPOLL_CLOEXEC, 1024, evbuf)
const sockfd = net.socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
const timerfd = sys.timer(1000, 1000)

timers[timerfd] = net.handle(timerfd, tbuf)
sockets[sockfd] = net.handle(sockfd)
loops[loopfd] = net.handle(loopfd)

r = net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1)
r = net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, 1)
r = net.bind(sockfd, '127.0.0.1', 3000)
r = net.listen(sockfd, SOMAXCONN)

r = loop.control(loopfd, EPOLL_CTL_ADD, timerfd, EPOLLIN)
r = loop.control(loopfd, EPOLL_CTL_ADD, sockfd, EPOLLIN)

r = loop.wait(loopfd, EVENTS, -1, evbuf)
while (r > 0) {
  for (let i = 0; i < r; i++) {
    onEvent(i * 2)
  }
  r = loop.wait(loopfd, EVENTS, -1, evbuf)
}

sys.free(rbuf)
sys.free(wbuf)
sys.free(evbuf)
sys.free(tbuf)
