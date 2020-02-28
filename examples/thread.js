const { net, sys, loop } = just
function threadMain () {
  const x = 100 * 1000
}
let source = threadMain.toString()
source = source.slice(source.indexOf('{') + 1, source.lastIndexOf('}')).trim()
const mem = new Float64Array(16)
const EVENTS = 1024
const evbuf = sys.calloc(EVENTS, 12)
const tbuf = sys.calloc(1, 8)
const events = new Uint32Array(evbuf)
const loopfd = loop.create(loop.EPOLL_CLOEXEC)
const timerfd = sys.timer(1000, 1000)
loop.control(loopfd, loop.EPOLL_CTL_ADD, timerfd, loop.EPOLLIN)
let r = 0
let rate = 0
let last = 0
while (1) {
  for (let j = 0; j < 10; j++) {
    just.thread.join(just.thread.spawn(source))
    rate++
  }
  r = loop.wait(loopfd, evbuf, 0)
  let off = 0
  for (let i = 0; i < r; i++) {
    const fd = events[off + 1]
    const event = events[off]
    if (event & loop.EPOLLIN) {
      const rss = sys.memoryUsage(mem)[0]
      const diff = rss - last
      last = rss
      just.print(`mem ${rss} diff ${diff} rate ${rate}`)
      net.read(fd, tbuf)
      rate = 0
    }
    off += 3
  }
}
