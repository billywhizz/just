const { sys, net } = just
const { EPOLLERR, EPOLLIN, EPOLLOUT, EPOLLHUP } = just.loop
const { IPPROTO_TCP, O_NONBLOCK, TCP_NODELAY, SO_KEEPALIVE, SOMAXCONN, AF_INET, SOCK_STREAM, SOL_SOCKET, SO_REUSEADDR, SO_REUSEPORT, SOCK_NONBLOCK } = net

function createServer (onConnect, opts = { bufSize: 16384 }) {
  function createSocket (fd, buf) {
    const socket = { fd, buf }
    socket.pull = (off = 0) => {
      const bytes = net.recv(fd, buf, off)
      if (bytes > 0) return bytes
      if (bytes < 0) {
        const errno = sys.errno()
        if (errno !== net.EAGAIN) {
          closeSocket(fd)
        }
        return
      }
      closeSocket(fd)
    }
    socket.write = (buf, len) => net.send(fd, buf, len)
    socket.onReadable = () => {}
    return socket
  }
  function closeSocket (fd) {
    clients[fd].onEnd()
    delete clients[fd]
    net.close(fd)
  }
  function onSocketEvent (fd, event) {
    if (event & EPOLLERR || event & EPOLLHUP) {
      closeSocket(fd)
      return
    }
    if (event & EPOLLIN) {
      clients[fd].onReadable()
      return
    }
    if (event & EPOLLOUT) {
      clients[fd].onWritable()
    }
  }
  function onListenEvent (fd, event) {
    const clientfd = net.accept(fd)
    net.setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, 1)
    net.setsockopt(clientfd, SOL_SOCKET, SO_KEEPALIVE, 1)
    let flags = sys.fcntl(clientfd, sys.F_GETFL, 0)
    flags |= O_NONBLOCK
    sys.fcntl(clientfd, sys.F_SETFL, flags)
    loop.add(clientfd, onSocketEvent)
    const sock = createSocket(clientfd, new ArrayBuffer(opts.bufSize))
    clients[clientfd] = sock
    onConnect(sock)
  }
  const { loop } = just.factory
  const clients = {}
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
    loop.add(fd, onListenEvent)
    return server
  }
  return server
}

module.exports = { createServer }
