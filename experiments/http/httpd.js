const { sys, net } = just
const { SOMAXCONN, AF_INET, SOCK_STREAM, SOL_SOCKET, SO_REUSEADDR, SO_REUSEPORT, SOCK_NONBLOCK } = net

function createServer (onConnect, opts = {}) {
  const server = Object.assign({
    maxPipeline: 256,
    reuseAddress: true,
    reusePort: true
  }, opts)
  const fd = net.socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
  if (fd <= 0) throw new Error(`Failed Creating Socket: ${sys.strerror(sys.errno())}`)
  if (server.reuseAddress && net.setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, 1) !== 0) throw new Error(`Failed Setting Reuse Address Socket Option: ${sys.strerror(sys.errno())}`)
  if (server.reusePort && net.setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, 1) !== 0) throw new Error(`Failed Setting Reuse Port Socket Option: ${sys.strerror(sys.errno())}`)
  server.listen = (address = '127.0.0.1', port = 3000, maxconn = SOMAXCONN) => {
    if (net.bind(fd, address, port) !== 0) throw new Error(`Failed Binding Socket: ${sys.strerror(sys.errno())}`)
    if (net.listen(fd, maxconn) !== 0) throw new Error(`Failed Listening on Socket: ${sys.strerror(sys.errno())}`)
    return server
  }
  return server
}

function main () {
  function onTimer () {
    const { rss } = just.memoryUsage()
    just.print(`rps ${rps} mem ${rss} conn ${conn}`)
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
    streams[clientfd] = new HTTPStream(buf)
    conn++
  }

  function onSocketEvent (fd, event) {
    if (event & EPOLLERR || event & EPOLLHUP) {
      net.close(fd)
      conn--
      return
    }
    const stream = streams[fd]
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

  const { sys, net } = just
  const { loop } = just.factory
  const { HTTPStream } = just.require('./http.js')
  const streams = {}
  const BUFSIZE = 16384
  const { EPOLLERR, EPOLLHUP } = just.loop
  const { SOMAXCONN, O_NONBLOCK, SOCK_STREAM, AF_INET, SOCK_NONBLOCK, SOL_SOCKET, SO_REUSEADDR, SO_REUSEPORT, IPPROTO_TCP, TCP_NODELAY, SO_KEEPALIVE } = net
  let conn = 0
  let rps = 0
  const MAX_PIPELINE = 512
  const response = 'HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n'
  const responseSize = response.length
  const wbuf = ArrayBuffer.fromString(response.repeat(MAX_PIPELINE))
  const server = createServer(onConnect).listen()

  const sockfd = net.socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
  net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1)
  net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, 1)
  net.bind(sockfd, '127.0.0.1', 3000)
  net.listen(sockfd, SOMAXCONN)
  loop.add(sockfd, onListenEvent)
  const timer = setTimeout(onTimer, 1000)
}

main()
