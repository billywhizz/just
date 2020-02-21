const { sys, net, loop, handle } = just

const handles = {}
global.errno = 0

function destroyHandle (fd, loopfd) {
  const h = handles[fd]
  if (!h) {
    just.print(`handle not found: ${fd}`)
    return
  }
  if (h.type === handle.SOCKET) {
    loop.control(loopfd, loop.EPOLL_CTL_DEL, fd)
  } else if (h.type === handle.TIMER) {
    loop.control(loopfd, loop.EPOLL_CTL_DEL, fd)
  }
  net.close(fd)
  handle.destroy(fd)
  delete handles[fd]
}

function main () {
  const BUFSIZE = 16384
  const EVENTS = 1024
  let rps = 0
  const mem = new Float64Array(16)
  const rbuf = sys.calloc(BUFSIZE, 1)
  const wbuf = sys.calloc(1, 'HTTP/1.1 200 OK\r\nContent-Length: 51\r\n\r\n<html><body><h1>Why hello there!</h1></body></html>')
  const evbuf = sys.calloc(2 * EVENTS, 4)
  const tbuf = sys.calloc(1, 8) // 64 bit number
  const events = new Uint32Array(evbuf)
  const loopfd = loop.create(loop.EPOLL_CLOEXEC, 1024, evbuf)
  const sockfd = net.socket(net.AF_INET, net.SOCK_STREAM | net.SOCK_NONBLOCK, 0)
  const timerfd = sys.timer(1000, 1000)
  handles[timerfd] = handle.create(timerfd, handle.TIMER, 1, tbuf)
  handles[sockfd] = handle.create(sockfd, handle.SOCKET)
  handles[loopfd] = handle.create(loopfd, handle.LOOP, EVENTS)
  let r = net.setsockopt(sockfd, net.SOL_SOCKET, net.SO_REUSEADDR, 1)
  r = net.setsockopt(sockfd, net.SOL_SOCKET, net.SO_REUSEPORT, 1)
  r = net.bind(sockfd, '127.0.0.1', 3000)
  r = net.listen(sockfd, net.SOMAXCONN)
  r = loop.control(loopfd, loop.EPOLL_CTL_ADD, timerfd, loop.EPOLLIN)
  r = loop.control(loopfd, loop.EPOLL_CTL_ADD, sockfd, loop.EPOLLIN)
  r = loop.wait(loopfd, EVENTS, -1, evbuf)
  while (r > 0) {
    for (let i = 0; i < r; i++) {
      const index = i * 2
      const fd = events[index]
      const event = events[index + 1]
      if (event & loop.EPOLLERR || event & loop.EPOLLHUP) {
        destroyHandle(fd, loopfd)
        continue
      }
      if (fd === sockfd) {
        const client = handle.create(net.accept(sockfd), handle.SOCKET, 1, rbuf, wbuf)
        let flags = sys.fcntl(client.fd, sys.F_GETFL, 0)
        flags |= net.O_NONBLOCK
        sys.fcntl(client.fd, sys.F_SETFL, flags)
        loop.control(loopfd, loop.EPOLL_CTL_ADD, client.fd, loop.EPOLLIN)
        handles[client.fd] = client
        continue
      }
      if (fd === timerfd) {
        const rss = sys.memoryUsage(mem)[0]
        just.print(`rps ${rps} mem ${rss}`)
        net.read(fd)
        rps = 0
        continue
      }
      if (event & loop.EPOLLOUT) {
        continue
      }
      if (event & loop.EPOLLIN) {
        const bytes = net.recv(fd)
        if (bytes === -1 && global.errno === net.EAGAIN) continue
        if (bytes === 0) {
          destroyHandle(fd, loopfd)
          continue
        }
        net.send(fd)
        rps++
      }
    }
    r = loop.wait(loopfd, EVENTS, -1, evbuf)
  }
  sys.free(rbuf)
  sys.free(wbuf)
  sys.free(evbuf)
  sys.free(tbuf)
}

main()
