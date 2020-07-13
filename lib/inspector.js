const ws = just.require('websocket')

const defaultOpts = {
  host: '127.0.0.1',
  port: 9222,
  favicon: 'https://nodejs.org/static/images/favicons/favicon.ico',
  onReady: () => {}
}

function createInspector (opts = defaultOpts) {
  const { host, port, onReady } = Object.assign(defaultOpts, opts)
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
    res.push('HTTP/1.1 101 Switching Protocols')
    res.push('Upgrade: websocket')
    res.push('Connection: Upgrade')
    res.push(`Sec-WebSocket-Accept: ${hash}`)
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
      const str = chunks.join('')
      global.send(str)
      chunks.length = 0
      try {
        if (JSON.parse(str).method === 'Runtime.runIfWaitingForDebugger') {
          request.isReady = true
          // this will block on the main event loop
          // todo. won't this be a problem if we are in middle of a chunk
          // of data?
        }
      } catch (err) {
        just.error('error parsing JSON')
        just.print(str)
        just.error(err.stack)
      }
    }
    request.onData = (off, len) => {
      request.parser.execute(new Uint8Array(rbuf, off, len), 0, len)
      if (request.isReady) {
        onReady()
        request.isReady = false
      }
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
    res.push('Cache-Control: no-cache')
    const payload = JSON.stringify({
      Browser: 'just.js/0.0.1',
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
    const fileName = just.args[1].split('/').slice(-1)[0]
    res.push('HTTP/1.1 200 OK')
    res.push('Content-Type: application/json; charset=UTF-8')
    res.push('Cache-Control: no-cache')
    const payload = JSON.stringify([{
      description: 'node.js instance',
      devtoolsFrontendUrl: `chrome-devtools://devtools/bundled/js_app.html?experiments=true&v8only=true&ws=${host}:${port}/${sessionId}`,
      devtoolsFrontendUrlCompat: `chrome-devtools://devtools/bundled/inspector.html?experiments=true&v8only=true&ws=${host}:${port}/${sessionId}`,
      faviconUrl: opts.favicon,
      id: sessionId,
      title: fileName,
      type: 'node',
      url: `file://${just.sys.cwd()}/${just.args[1]}`,
      webSocketDebuggerUrl: `ws://${host}:${port}/${sessionId}`
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
      const count = http.parseRequests(rbuf, bytes, 0)
      if (count > 0) {
        const requests = http.getRequests()
        for (const request of requests) {
          if (request.method !== 'GET') {
            net.send(fd, wbuf, sys.writeString(wbuf, getNotFound()))
            continue
          }
          request.fd = fd
          if (request.url === '/json' || request.url === '/json/list') {
            net.send(fd, wbuf, sys.writeString(wbuf, getJSON()))
            continue
          }
          if (request.url === '/json/version') {
            net.send(fd, wbuf, sys.writeString(wbuf, getJSONVersion()))
            continue
          }
          const { Upgrade } = request.headers
          if (Upgrade && Upgrade.toLowerCase() === 'websocket') {
            startWebSocket(request)
            continue
          }
          net.send(fd, wbuf, sys.writeString(wbuf, getNotFound()))
        }
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

  global.receive = message => {
    net.send(clientfd, ws.createMessage(message))
  }

  global.onRunMessageLoop = () => {
    paused = true
    // v8 inspector is telling us to break/pause
    // we go into a blocking loop here and only process events
    // on the inspectors event loop, which will block any further
    // processing of the main or other event loop(s)
    while (paused) {
      loop.poll(1)
      just.sys.runMicroTasks()
    }
  }

  global.onQuitMessageLoop = () => {
    // v8 inspector is telling us to quit our dedicated event loop
    // we set this flag so onRunMessageLoop returns
    // and the main or other loop(s) can continue
    paused = false
  }

  just.inspector.enable()

  const { crypto, encode, sys, net, http } = just
  const websockets = {}
  const BUFSIZE = 1 * 1024 * 1024
  const { EPOLLIN, EPOLLERR, EPOLLHUP } = just.loop
  const {
    SOMAXCONN,
    O_NONBLOCK,
    SOCK_STREAM,
    AF_INET,
    SOCK_NONBLOCK,
    SOL_SOCKET,
    SO_REUSEADDR,
    SO_REUSEPORT,
    IPPROTO_TCP,
    TCP_NODELAY,
    SO_KEEPALIVE
  } = net
  let paused = false
  let clientfd = 0
  const loop = just.factory.create(128)
  const rbuf = new ArrayBuffer(BUFSIZE)
  const wbuf = new ArrayBuffer(BUFSIZE)
  const sockfd = net.socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
  net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1)
  net.setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, 1)
  net.bind(sockfd, host, port)
  net.listen(sockfd, SOMAXCONN)
  loop.add(sockfd, onListenEvent)
  return {
    loop,
    sockfd,
    quit: () => {
      net.close(sockfd)
    },
    info: () => {
      return { paused }
    }
  }
}

module.exports = { createInspector }
