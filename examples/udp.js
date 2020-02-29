const { sys, net, loop, udp, args } = just
const { EPOLL_CLOEXEC, EPOLLIN, EPOLL_CTL_ADD } = loop
const { SOCK_DGRAM, AF_INET, SOCK_NONBLOCK } = net
const { sendmsg, recvmsg } = udp

const EVENTS = 1024
const handlers = {}

function onTimerEvent (fd, event) {
  const r = sendmsg(sockfd, buf, '127.0.0.1', dest)
  just.print(`sendmsg: ${r}`)
  net.read(fd, tbuf)
}

function onDgramEvent (fd, event) {
  just.print('hello')
  const address = []
  const r = recvmsg(fd, buf, address)
  just.print(`recvmsg: ${r}`)
  just.print(address[0])
  just.print(address[1])
}

const buf = sys.calloc(1, 'hello')
const timerfd = sys.timer(1000, 1000)
handlers[timerfd] = onTimerEvent
const tbuf = sys.calloc(1, 8)
const evbuf = sys.calloc(EVENTS, 12)
const events = new Uint32Array(evbuf)
const loopfd = loop.create(EPOLL_CLOEXEC)
const sockfd = net.socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0)
handlers[sockfd] = onDgramEvent
const source = parseInt(args[2] || 4444, 10)
const dest = parseInt(args[3] || 5555, 10)
let r = net.bind(sockfd, '127.0.0.1', source)
r = loop.control(loopfd, EPOLL_CTL_ADD, sockfd, EPOLLIN)
r = loop.control(loopfd, EPOLL_CTL_ADD, timerfd, EPOLLIN)
r = loop.wait(loopfd, evbuf)
while (r > 0) {
  let off = 0
  for (let i = 0; i < r; i++) {
    const fd = events[off + 1]
    handlers[fd](fd, events[off])
    off += 3
  }
  r = loop.wait(loopfd, evbuf)
}
