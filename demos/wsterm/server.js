const websockets = {}
const rbuf = new ArrayBuffer(1 * 1024 * 1024)
const wbuf = new ArrayBuffer(1 * 1024 * 1024)
const { Parser, createBinaryMessage } = just.require('websocket')
const { crypto, encode, sys, net, http } = just
const { EPOLLIN, EPOLLERR, EPOLLHUP } = just.loop
const { SOMAXCONN, O_NONBLOCK, SOCK_STREAM, AF_UNIX, AF_INET, SOCK_NONBLOCK, SOL_SOCKET, SO_REUSEADDR, SO_REUSEPORT, IPPROTO_TCP, TCP_NODELAY, SO_KEEPALIVE, socketpair } = net

function sendResponse (request, statusCode = 200, statusMessage = 'OK', str = '') {
  const { fd } = request
  const headers = []
  let len = 0
  if (str.length) {
    len = sys.writeString(wbuf, str)
  }
  headers.push(`HTTP/1.1 ${statusCode} ${statusMessage}`)
  headers.push(`Content-Length: ${len}`)
  headers.push('')
  headers.push('')
  const hstr = headers.join('\r\n')
  const buf = ArrayBuffer.fromString(hstr)
  let bytes = net.send(fd, buf)
  if (!str.length) return
  const chunks = Math.ceil(len / 16384)
  let total = 0
  for (let i = 0, o = 0; i < chunks; ++i, o += 16384) {
    bytes = net.send(fd, wbuf.slice(o, o + 16384))
    total += bytes
  }
  just.print(total)
}

function closeSocket (fd) {
  net.close(fd)
  delete websockets[fd]
}

function sha1 (str) {
  const source = new ArrayBuffer(str.length)
  const len = sys.writeString(source, str)
  const dest = new ArrayBuffer(64)
  crypto.hash(crypto.SHA1, source, dest, len)
  const b64Length = encode.base64Encode(dest, source, 20)
  return sys.readString(source, b64Length)
}

function startWebSocket (request) {
  const { fd, url, headers } = request
  request.sessionId = url.slice(1)
  const key = headers['Sec-WebSocket-Key']
  const hash = sha1(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`)
  const res = []
  res.push('HTTP/1.1 101 Upgrade')
  res.push('Upgrade: websocket')
  res.push('Connection: Upgrade')
  res.push(`Sec-WebSocket-Accept: ${hash}`)
  res.push('Content-Length: 0')
  res.push('')
  res.push('')
  websockets[fd] = request
  const stdinfds = []
  const stdoutfds = []
  // todo: can i just one socketpair?
  socketpair(AF_UNIX, SOCK_STREAM, stdinfds)
  socketpair(AF_UNIX, SOCK_STREAM, stdoutfds)
  const repl = new ArrayBuffer(1 * 1024 * 1024)
  just.require('repl').repl(loop, repl, stdinfds[1], stdoutfds[1])
  loop.add(stdoutfds[0], (fd, event) => {
    if (event & net.EPOLLERR || event & net.EPOLLHUP) {
      net.close(fd)
      return
    }
    if (event && EPOLLIN) {
      const bytes = net.read(fd, repl)
      const msg = createBinaryMessage(repl, bytes)
      net.send(request.fd, msg)
    }
  })
  const parser = new Parser()
  parser.onChunk = (off, len, header) => {
    let size = len
    let pos = 0
    const bytes = new Uint8Array(rbuf, off, len)
    while (size--) {
      bytes[pos] = bytes[pos] ^ header.maskkey[pos % 4]
      pos++
    }
    //const msg = createBinaryMessage(rbuf, len, off)
    //net.write(request.fd, msg)
    net.write(stdinfds[0], rbuf, len, off)
  }
  request.onData = (off, len) => {
    request.parser.execute(new Uint8Array(rbuf, off, len), 0, len)
  }
  request.parser = parser
  net.send(fd, wbuf, sys.writeString(wbuf, res.join('\r\n')))
}

function onSocketEvent (fd, event) {
  if (event & EPOLLERR || event & EPOLLHUP) return closeSocket(fd)
  if (event & EPOLLIN) {
    const bytes = net.recv(fd, rbuf)
    if (bytes < 0) {
      const errno = sys.errno()
      if (errno !== net.EAGAIN) {
        just.print(`recv error: ${sys.strerror(errno)} (${errno})`)
        closeSocket(fd)
      }
      return
    }
    if (bytes === 0) return closeSocket(fd)
    if (websockets[fd]) return websockets[fd].onData(0, bytes)
    const nread = http.parseRequest(rbuf, bytes, 0)
    if (nread < 0) {
      just.print('omg')
      return closeSocket(fd)
    }
    const request = http.getRequest()
    request.fd = fd
    if (request.method !== 'GET') {
      return sendResponse(request, 403, 'Forbidden')
    }
    if (request.headers.Upgrade && request.headers.Upgrade.toLowerCase() === 'websocket') {
      return startWebSocket(request)
    }
    if (request.url === '/' || request.url === '/index.html') {
      return sendResponse(request, 200, 'OK', just.fs.readFile('index.html'))
    }
    if (request.url === '/term.js') {
      return sendResponse(request, 200, 'OK', just.fs.readFile('term.js'))
    }
    sendResponse(request, 404, 'Not Found')
  }
}

function onListenEvent (fd, event) {
  const clientfd = net.accept(fd)
  net.setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, 1)
  net.setsockopt(clientfd, SOL_SOCKET, SO_KEEPALIVE, 1)
  loop.add(clientfd, onSocketEvent)
  let flags = sys.fcntl(clientfd, sys.F_GETFL, 0)
  flags |= O_NONBLOCK
  sys.fcntl(clientfd, sys.F_SETFL, flags)
  loop.update(clientfd, EPOLLIN)
}

function main () {
  const sockfd = net.socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
  net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1)
  net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, 1)
  net.bind(sockfd, '127.0.0.1', 8888)
  net.listen(sockfd, SOMAXCONN)
  loop.add(sockfd, onListenEvent)
}

main()
