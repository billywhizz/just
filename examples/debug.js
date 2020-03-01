function DebuggerAgent () {
  const { crypto, encode, sys, net, loop, http } = just
  const ws = just.require('./websocket.js')
  const ipc = just.require('./ipc.js')
  const BUFSIZE = 1 * 1024 * 1024
  const EVENTS = 128
  const { EPOLL_CLOEXEC, EPOLL_CTL_ADD, EPOLLIN, EPOLLERR, EPOLLHUP } = loop
  const { SOMAXCONN, O_NONBLOCK, SOCK_STREAM, AF_INET, SOCK_NONBLOCK, SOL_SOCKET, SO_REUSEADDR, SO_REUSEPORT, IPPROTO_TCP, TCP_NODELAY, SO_KEEPALIVE } = net
  const handlers = {}
  const ipcParser = new ipc.Parser()
  let clientfd = 0

  function onIPCData (fd, event) {
    if (event & EPOLLIN) {
      const bytes = net.read(fd, rbuf)
      ipcParser.execute(rbuf, bytes, message => net.send(clientfd, ws.createMessage(message)))
    }
  }

  function onListenEvent (fd, event) {
    const clientfd = net.accept(fd)
    net.setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, 1)
    net.setsockopt(clientfd, SOL_SOCKET, SO_KEEPALIVE, 1)
    handlers[clientfd] = onSocketEvent
    let flags = sys.fcntl(clientfd, sys.F_GETFL, 0)
    flags |= O_NONBLOCK
    sys.fcntl(clientfd, sys.F_SETFL, flags)
    loop.control(loopfd, EPOLL_CTL_ADD, clientfd, EPOLLIN)
  }

  function sha1 (str) {
    const source = new ArrayBuffer(str.length)
    const len = sys.writeString(source, str)
    const dest = new ArrayBuffer(64)
    crypto.hash(crypto.SHA1, source, dest, len)
    const b64Length = encode.base64Encode(dest, source, 20)
    return sys.readString(source, b64Length)
  }

  const websockets = {}

  function startWebSocket (request) {
    const { fd, url, headers } = request
    request.sessionId = url.slice(1)
    const key = headers['Sec-WebSocket-Key']
    const hash = sha1(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`)
    const res = []
    res.push('HTTP/1.1 200 OK')
    res.push('Upgrade: websocket')
    res.push('Connection: Upgrade')
    res.push(`Sec-WebSocket-Accept: ${hash}`)
    res.push('Content-Length: 0')
    res.push('')
    res.push('')
    websockets[fd] = request
    const parser = new ws.Parser()
    const chunks = []
    parser.onHeader = header => {
      chunks.length = 0
    }
    parser.onChunk = (off, len, header) => {
      let size = len
      let pos = 0
      const bytes = new Uint8Array(rbuf, off, len)
      while (size--) {
        bytes[pos] = bytes[pos] ^ header.maskkey[pos % 4]
        pos++
      }
      chunks.push(rbuf.readString(len, off))
    }
    parser.onMessage = header => {
      const payload = chunks.join('')
      const ab = new ArrayBuffer(payload.length + 1)
      ab.writeString(payload)
      net.send(ipcfd, ab)
      chunks.length = 0
    }
    request.onData = (off, len) => {
      const u8 = new Uint8Array(rbuf, off, len)
      request.parser.execute(u8, 0, len)
    }
    request.parser = parser
    clientfd = fd
    net.send(fd, wbuf, sys.writeString(wbuf, res.join('\r\n')))
  }

  function getNotFound () {
    const res = []
    res.push('HTTP/1.1 404 Not Found')
    res.push('Content-Length: 0')
    res.push('')
    res.push('')
    return res.join('\r\n')
  }

  function getJSONVersion () {
    const res = []
    res.push('HTTP/1.1 200 OK')
    res.push('Content-Type: application/json; charset=UTF-8')
    const payload = JSON.stringify({
      Browser: 'node.js/v13.9.0',
      'Protocol-Version': '1.1'
    })
    res.push(`Content-Length: ${payload.length}`)
    res.push('')
    res.push(payload)
    return res.join('\r\n')
  }

  function getSession () {
    return '247644ae-08ea-4311-a986-7a1b1750d5cf'
  }

  function getJSON () {
    const res = []
    const sessionId = getSession()
    res.push('HTTP/1.1 200 OK')
    res.push('Content-Type: application/json; charset=UTF-8')
    const payload = JSON.stringify([{
      description: 'node.js instance',
      devtoolsFrontendUrl: `chrome-devtools://devtools/bundled/js_app.html?experiments=true&v8only=true&ws=127.0.0.1:9222/${sessionId}`,
      devtoolsFrontendUrlCompat: `chrome-devtools://devtools/bundled/inspector.html?experiments=true&v8only=true&ws=127.0.0.1:9222/${sessionId}`,
      faviconUrl: 'https://nodejs.org/static/favicon.ico',
      id: sessionId,
      title: 'debug.js',
      type: 'node',
      url: `file://${just.sys.cwd()}/debug.js`,
      webSocketDebuggerUrl: `ws://127.0.0.1:9222/${sessionId}`
    }])
    res.push(`Content-Length: ${payload.length}`)
    res.push('')
    res.push(payload)
    return res.join('\r\n')
  }

  function onSocketEvent (fd, event) {
    if (event & EPOLLERR || event & EPOLLHUP) {
      net.close(fd)
      delete websockets[fd]
      return
    }
    const bytes = net.recv(fd, rbuf)
    if (bytes > 0) {
      if (websockets[fd]) {
        websockets[fd].onData(0, bytes)
        return
      }
      const nread = http.parseRequest(rbuf, bytes, 0)
      if (nread > 0) {
        const request = http.getRequest()
        if (request.method !== 'GET') {
          net.send(fd, wbuf, sys.writeString(wbuf, getNotFound()))
          return
        }
        request.fd = fd
        if (request.url === '/json' || request.url === '/json/list') {
          net.send(fd, wbuf, sys.writeString(wbuf, getJSON()))
          return
        }
        if (request.url === '/json/version') {
          net.send(fd, wbuf, sys.writeString(wbuf, getJSONVersion()))
          return
        }
        if (request.headers.Upgrade && request.headers.Upgrade.toLowerCase() === 'websocket') {
          startWebSocket(request)
          return
        }
        net.send(fd, wbuf, sys.writeString(wbuf, getNotFound()))
      } else {
        just.print('OHNO!')
      }
      return
    }
    if (bytes < 0) {
      const errno = sys.errno()
      if (errno !== net.EAGAIN) {
        just.print(`recv error: ${sys.strerror(errno)} (${errno})`)
        net.close(fd)
        delete websockets[fd]
      }
      return
    }
    net.close(fd)
    delete websockets[fd]
  }

  const rbuf = new ArrayBuffer(BUFSIZE)
  const wbuf = new ArrayBuffer(BUFSIZE)
  const evbuf = new ArrayBuffer(EVENTS * 12)
  const events = new Uint32Array(evbuf)
  const loopfd = loop.create(EPOLL_CLOEXEC)
  const sockfd = net.socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
  const ipcfd = just.fd
  handlers[sockfd] = onListenEvent
  handlers[ipcfd] = onIPCData
  let r = net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1)
  r = net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, 1)
  r = net.bind(sockfd, '127.0.0.1', 9222)
  r = net.listen(sockfd, SOMAXCONN)
  r = loop.control(loopfd, EPOLL_CTL_ADD, ipcfd, EPOLLIN)
  r = loop.control(loopfd, EPOLL_CTL_ADD, sockfd, EPOLLIN)
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
}

function createInspector (loop, onReady) {
  const { sys, net } = just
  const { EPOLLIN } = just.loop
  const ipc = just.require('./ipc.js')
  just.inspector.enable()
  let source = DebuggerAgent.toString()
  source = source.slice(source.indexOf('{') + 1, source.lastIndexOf('}')).trim()
  const fds = []
  net.socketpair(net.AF_UNIX, net.SOCK_STREAM, fds)
  const shared = new SharedArrayBuffer(1024)
  const threadName = `${just.path.join(sys.cwd(), just.args[1])}#DebuggerAgent`
  const ipcfd = fds[0]
  let paused = false
  just.thread.spawn(source, shared, fds[1], threadName)
  global.receive = message => {
    const ab = new ArrayBuffer(message.length + 1)
    ab.writeString(message)
    net.send(ipcfd, ab)
  }
  global.onRunMessageLoop = () => {
    paused = true
    while (paused) {
      loop.poll(1)
      just.sys.runMicroTasks()
    }
  }
  global.onQuitMessageLoop = () => {
    paused = false
  }
  const BUFSIZE = 1 * 1024 * 1024
  const rbuf = new ArrayBuffer(BUFSIZE)
  const ipcParser = new ipc.Parser()
  loop.add(ipcfd, (fd, event) => {
    if (event & EPOLLIN) {
      const bytes = net.read(fd, rbuf)
      ipcParser.execute(rbuf, bytes, message => {
        const json = JSON.parse(message)
        if (json.method === 'Runtime.runIfWaitingForDebugger') {
          onReady()
        }
        global.send(message)
      })
    }
  })
  while (loop.count > 0) {
    if (paused) {
      just.usleep(1000)
    } else {
      loop.poll(1)
    }
    just.sys.runMicroTasks()
  }
}

const loop = just.createLoop(128)
createInspector(loop, () => {
  just.print('debugger ready')
  const { args, fs, path, sys } = just
  just.vm.runScript(fs.readFile(args[2]), path.join(sys.cwd(), args[2]))
})
