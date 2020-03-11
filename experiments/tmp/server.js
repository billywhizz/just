function main () {
  const { sys, net } = just
  const { loop } = just.factory
  const { createParser } = just.require('./parser.js')
  const clients = {}
  let rps = 0
  let conn = 0
  const BUFSIZE = 16384
  const { EPOLLERR, EPOLLHUP } = just.loop
  const { SOMAXCONN, O_NONBLOCK, SOCK_STREAM, AF_INET, SOCK_NONBLOCK, SOL_SOCKET, SO_REUSEADDR, SO_REUSEPORT, IPPROTO_TCP, TCP_NODELAY, SO_KEEPALIVE } = net

  function onTimerEvent (fd, event) {
    const rss = sys.memoryUsage(mem)[0]
    just.print(`rps ${rps} mem ${rss} conn ${conn}`)
    net.read(fd, tbuf)
    rps = 0
  }

  function onListenEvent (fd, event) {
    const clientfd = net.accept(fd)
    net.setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, 1)
    net.setsockopt(clientfd, SOL_SOCKET, SO_KEEPALIVE, 1)
    let flags = sys.fcntl(clientfd, sys.F_GETFL, 0)
    flags |= O_NONBLOCK
    sys.fcntl(clientfd, sys.F_SETFL, flags)
    loop.add(clientfd, onSocketEvent)
    const buf = new ArrayBuffer(BUFSIZE)
    clients[clientfd] = { buf, offset: 0, parser: createParser(buf) }
    conn++
  }

  function onSocketEvent (fd, event) {
    if (event & EPOLLERR || event & EPOLLHUP) {
      net.close(fd)
      conn--
      return
    }
    const client = clients[fd]
    const { buf, parser } = client
    let { offset } = client
    const bytes = net.recv(fd, buf, offset)
    if (bytes > 0) {
      const size = offset + bytes
      const { count, off } = parser.parse(size)
      if (count > 0) {
        net.send(fd, wbuf, count * responseSize)
        rps += count
        if (off < (size)) {
          buf.copyFrom(buf, 0, bytes - off, off)
          offset = bytes - off
        } else {
          offset = 0
        }
      } else {
        if (size === buf.byteLength) {
          throw new Error('Request Too Big')
        }
        offset = size
      }
      client.offset = offset
      return
    }
    if (bytes < 0) {
      const errno = sys.errno()
      if (errno !== net.EAGAIN) {
        just.print(`recv error: ${sys.strerror(errno)} (${errno})`)
        net.close(fd)
        conn--
      }
      return
    }
    net.close(fd)
    conn--
  }

  const mem = new Float64Array(16)
  const MAX_PIPELINE = 512
  const response = 'HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n'
  const responseSize = response.length
  const wbuf = ArrayBuffer.fromString(response.repeat(MAX_PIPELINE))
  const tbuf = new ArrayBuffer(8)
  const timerfd = sys.timer(1000, 1000)
  const sockfd = net.socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
  net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1)
  net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, 1)
  net.bind(sockfd, '127.0.0.1', 3000)
  net.listen(sockfd, SOMAXCONN)
  loop.add(timerfd, onTimerEvent)
  loop.add(sockfd, onListenEvent)
}

main()
