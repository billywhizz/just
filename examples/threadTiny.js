function threadTiny () {
  function onListenEvent (fd, event) {
    if (event & EPOLLERR || event & EPOLLHUP) {
      delete buffers[fd]
      net.close(fd)
      return
    }
    const clientfd = net.accept(fd)
    net.setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, 1)
    net.setsockopt(clientfd, SOL_SOCKET, SO_KEEPALIVE, 1)
    loop.add(clientfd, onSocketEvent)
    let flags = sys.fcntl(clientfd, sys.F_GETFL, 0)
    flags |= O_NONBLOCK
    sys.fcntl(clientfd, sys.F_SETFL, flags)
    loop.update(clientfd, EPOLLIN)
    const buffer = buffers[clientfd] = new ArrayBuffer(BUFFER_SIZE)
    buffer.offset = 0
    conn++
  }

  function onSocketEvent (fd, event) {
    if (event & EPOLLERR || event & EPOLLHUP) {
      net.close(fd)
      conn--
      return
    }
    const { buf, size } = responses[200]
    const buffer = buffers[fd]
    const bytes = net.recv(fd, buffer, buffer.offset, BUFFER_SIZE - buffer.offset)
    reads[bytes] = reads[bytes] ? reads[bytes] + 1 : 1
    bps += bytes
    if (bytes > 0) {
      const count = parseRequests(buffer, buffer.offset + bytes, 0, answer)
      counts[count] = counts[count] ? counts[count] + 1 : 1
      if (count > 0) {
        rps += count
        wps += net.send(fd, buf, size * count)
      }
      if (answer[0] > 0) {
        const start = buffer.offset + bytes - answer[0]
        const len = answer[0]
        if (start > buffer.offset) {
          buffer.copyFrom(buffer, 0, len, start)
        }
        buffer.offset = len
        return
      }
      buffer.offset = 0
      return
    }
    if (bytes < 0) {
      const errno = sys.errno()
      if (errno !== net.EAGAIN) {
        just.print(`recv error: ${sys.strerror(errno)} (${errno})`)
        delete buffers[fd]
        net.close(fd)
        conn--
      }
      return
    }
    delete buffers[fd]
    net.close(fd)
    conn--
  }

  function onTimer () {
    const { rss } = memoryUsage()
    const { user, system } = cpuUsage()
    const upc = ((user - last.user) / 1000000).toFixed(2)
    const spc = ((system - last.system) / 1000000).toFixed(2)
    if (rps > max) max = rps
    just.print(`rps ${rps} max ${max} mem ${rss} conn ${conn} cpu ${upc} / ${spc} Gbps  r ${((bps * 8) / bw).toFixed(2)} w ${((wps * 8) / bw).toFixed(2)}`)
    rps = 0
    last.user = user
    last.system = system
    bps = 0
    wps = 0
  }

  const { sys, net, http, memoryUsage, cpuUsage } = just
  const { EPOLLIN, EPOLLERR, EPOLLHUP } = just.loop
  const { IPPROTO_TCP, O_NONBLOCK, TCP_NODELAY, SO_KEEPALIVE, SOMAXCONN, AF_INET, SOCK_STREAM, SOL_SOCKET, SO_REUSEADDR, SO_REUSEPORT, SOCK_NONBLOCK } = net
  const { parseRequests } = http

  const maxPipeline = http.MAX_PIPELINE
  const r200 = 'HTTP/1.1 200 OK\r\nServer: V\r\nDate: Sat, 27 Jun 2020 01:11:23 GMT\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!'
  const responses = {
    200: {
      buf: ArrayBuffer.fromString(r200.repeat(maxPipeline)),
      size: r200.length
    }
  }
  let rps = 0
  let bps = 0
  let wps = 0
  let conn = 0
  const { loop } = just.factory
  const last = { user: 0, system: 0 }
  const bw = 1000 * 1000 * 1000 // Gigabit
  const buffers = {}
  const BUFFER_SIZE = 16384
  const reads = {}
  let max = 0
  const answer = [0]
  const counts = {}

  just.setInterval(onTimer, 1000)
  const sockfd = net.socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
  net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1)
  net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, 1)
  net.bind(sockfd, '127.0.0.1', 3000)
  net.listen(sockfd, SOMAXCONN)
  loop.add(sockfd, onListenEvent)
}

let source = threadTiny.toString()
source = source.slice(source.indexOf('{') + 1, source.lastIndexOf('}')).trim()
const threads = []
threads.push(just.thread.spawn(source))
threads.push(just.thread.spawn(source))
threads.push(just.thread.spawn(source))
threads.push(just.thread.spawn(source))

const timer = just.setInterval(() => {
  //just.print(JSON.stringify(threads.map(v => Number(v))))
}, 1000)
