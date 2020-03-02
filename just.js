const { vm, fs, sys, net } = just

function wrapHrtime (hrtime) {
  const time = new BigUint64Array(1)
  return () => {
    sys.hrtime(time)
    return time[0]
  }
}

function wrapCpuUsage (cpuUsage) {
  const cpu = new Float64Array(16)
  return () => {
    sys.cpuUsage(cpu)
    return {
      user: cpu[0],
      system: cpu[1]
    }
  }
}

function wrapMemoryUsage (memoryUsage) {
  const mem = new Float64Array(16)
  return () => {
    sys.memoryUsage(mem)
    return {
      rss: mem[0],
      total_heap_size: mem[1],
      used_heap_size: mem[2],
      external_memory: mem[3],
      heap_size_limit: mem[5],
      total_available_size: mem[10],
      total_heap_size_executable: mem[11],
      total_physical_size: mem[12]
    }
  }
}

function wrapEnv (env) {
  return () => {
    return env()
      .map(entry => entry.split('='))
      .reduce((e, pair) => { e[pair[0]] = pair[1]; return e }, {})
  }
}

function wrapHeapUsage (heapUsage) {
  const heap = [new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4), new Float64Array(4)]
  return () => {
    const usage = heapUsage(heap)
    usage.spaces = Object.keys(usage.heapSpaces).map(k => {
      const space = usage.heapSpaces[k]
      return {
        name: k,
        size: space[2],
        used: space[3],
        available: space[1],
        physicalSize: space[0]
      }
    })
    delete usage.heapSpaces
    return usage
  }
}

function createLoop (nevents = 1024) {
  const {
    create, wait, control, EPOLL_CLOEXEC, EPOLL_CTL_ADD,
    EPOLL_CTL_DEL, EPOLL_CTL_MOD, EPOLLIN
  } = just.loop
  const evbuf = new ArrayBuffer(nevents * 12)
  const events = new Uint32Array(evbuf)
  const loopfd = create(EPOLL_CLOEXEC)
  const handles = {}
  function poll (timeout = -1, sigmask) {
    let r = 0
    if (sigmask) {
      r = wait(loopfd, evbuf, timeout, sigmask)
    } else {
      r = wait(loopfd, evbuf, timeout)
    }
    if (r > 0) {
      let off = 0
      for (let i = 0; i < r; i++) {
        const fd = events[off + 1]
        handles[fd](fd, events[off])
        off += 3
      }
    }
    return r
  }
  function add (fd, callback, events = EPOLLIN) {
    const r = control(loopfd, EPOLL_CTL_ADD, fd, events)
    if (r === 0) {
      handles[fd] = callback
      instance.count++
    }
    return r
  }
  function remove (fd) {
    const r = control(loopfd, EPOLL_CTL_DEL, fd)
    if (r === 0) {
      delete handles[fd]
      instance.count--
    }
    return r
  }
  function update (fd, events = EPOLLIN) {
    const r = control(loopfd, EPOLL_CTL_MOD, fd, events)
    return r
  }
  const instance = { fd: loopfd, poll, add, remove, update, handles, count: 0 }
  return instance
}

const factory = {
  loops: [createLoop(1024)],
  paused: true,
  create: (nevents = 128) => {
    const loop = createLoop(nevents)
    factory.loops.push(loop)
    return loop
  },
  run: (ms = 1) => {
    factory.paused = false
    while (!factory.paused) {
      let total = 0
      for (const loop of factory.loops) {
        if (loop.count > 0) loop.poll(ms)
        total += loop.count
      }
      sys.runMicroTasks()
      if (total === 0) {
        factory.paused = true
        break
      }
    }
  },
  stop: () => {
    factory.paused = true
  },
  shutdown: () => {

  }
}

function repl (loop, buf, onExpression) {
  const { EPOLLIN } = just.loop
  const { O_NONBLOCK, EAGAIN } = just.net
  const stdin = 0
  sys.fcntl(stdin, sys.F_SETFL, (sys.fcntl(stdin, sys.F_GETFL, 0) | O_NONBLOCK))
  loop.add(stdin, (fd, event) => {
    if (event & EPOLLIN) {
      const bytes = net.read(fd, buf)
      if (bytes < 0) {
        const err = sys.errno()
        if (err !== EAGAIN) {
          just.print(`read error: ${sys.strerror(err)} (${err})`)
          net.close(fd)
        }
        return
      }
      onExpression(buf.readString(bytes).trim())
    }
  })
}

function pathModule () {
  const CHAR_FORWARD_SLASH = 47
  const CHAR_BACKWARD_SLASH = 92
  const CHAR_DOT = 46

  function baseName (path) {
    return path.slice(0, path.lastIndexOf('/') + 1)
  }

  function isPathSeparator (code) {
    return code === CHAR_FORWARD_SLASH || code === CHAR_BACKWARD_SLASH
  }

  function isPosixPathSeparator (code) {
    return code === CHAR_FORWARD_SLASH
  }

  function normalizeString (path, allowAboveRoot, separator) {
    let res = ''
    let lastSegmentLength = 0
    let lastSlash = -1
    let dots = 0
    let code = 0
    for (let i = 0; i <= path.length; ++i) {
      if (i < path.length) {
        code = path.charCodeAt(i)
      } else if (isPathSeparator(code)) {
        break
      } else {
        code = CHAR_FORWARD_SLASH
      }
      if (isPathSeparator(code)) {
        if (lastSlash === i - 1 || dots === 1) {
          // NOOP
        } else if (dots === 2) {
          if (res.length < 2 || lastSegmentLength !== 2 ||
              res.charCodeAt(res.length - 1) !== CHAR_DOT ||
              res.charCodeAt(res.length - 2) !== CHAR_DOT) {
            if (res.length > 2) {
              const lastSlashIndex = res.lastIndexOf(separator)
              if (lastSlashIndex === -1) {
                res = ''
                lastSegmentLength = 0
              } else {
                res = res.slice(0, lastSlashIndex)
                lastSegmentLength = res.length - 1 - res.lastIndexOf(separator)
              }
              lastSlash = i
              dots = 0
              continue
            } else if (res.length !== 0) {
              res = ''
              lastSegmentLength = 0
              lastSlash = i
              dots = 0
              continue
            }
          }
          if (allowAboveRoot) {
            res += res.length > 0 ? `${separator}..` : '..'
            lastSegmentLength = 2
          }
        } else {
          if (res.length > 0) {
            res += `${separator}${path.slice(lastSlash + 1, i)}`
          } else {
            res = path.slice(lastSlash + 1, i)
          }
          lastSegmentLength = i - lastSlash - 1
        }
        lastSlash = i
        dots = 0
      } else if (code === CHAR_DOT && dots !== -1) {
        ++dots
      } else {
        dots = -1
      }
    }
    return res
  }

  function normalize (path) {
    if (path.length === 0) return '.'
    const isAbsolute = path.charCodeAt(0) === CHAR_FORWARD_SLASH
    const sep = path.charCodeAt(path.length - 1) === CHAR_FORWARD_SLASH
    path = normalizeString(path, !isAbsolute, '/', isPosixPathSeparator)
    if (path.length === 0) {
      if (isAbsolute) return '/'
      return sep ? './' : '.'
    }
    if (sep) path += '/'
    return isAbsolute ? `/${path}` : path
  }

  function join (...args) {
    if (args.length === 0) return '.'
    if (args.length === 2 && args[1][0] === '/') return normalize(args[1])
    let joined
    for (let i = 0; i < args.length; ++i) {
      const arg = args[i]
      if (arg.length > 0) {
        if (joined === undefined) {
          joined = arg
        } else {
          joined += `/${arg}`
        }
      }
    }
    if (joined === undefined) return '.'
    return normalize(joined)
  }

  return { join, baseName, normalize }
}

function wrapRequire (cache = {}, pathMod = pathModule()) {
  function require (path, parent) {
    const { join, baseName } = pathMod
    let dirName = parent ? parent.dirName : baseName(join(sys.cwd(), just.args[1] || './'))
    const fileName = join(dirName, path)
    if (cache[fileName]) return cache[fileName].exports
    dirName = baseName(fileName)
    const params = ['exports', 'require', 'module']
    const exports = {}
    const module = { exports, dirName, fileName }
    module.text = just.fs.readFile(fileName)
    const fun = just.vm.compile(module.text, fileName, params, [])
    module.function = fun
    cache[fileName] = module
    fun.call(exports, exports, p => require(p, module), module)
    return module.exports
  }
  return { cache, require }
}

function wsModule () {
  function HYBIMessage () {
    this.FIN = 1
    this.RSV1 = 0
    this.RSV2 = 0
    this.RSV3 = 0
    this.OpCode = 1
    this.length = 0
  }

  function Parser () {
    var _parser = this
    var current = new HYBIMessage()
    var pos = 0
    var bpos = 0
    var _complete = false
    var _inheader = true
    var _payload16 = false
    var _payload64 = false

    function onHeader () {
      if (_parser.onHeader) _parser.onHeader(current)
    }

    function onMessage () {
      if (_parser.onMessage) _parser.onMessage(current)
      pos = 0
      _complete = true
    }

    function onChunk (off, len, header) {
      if (_parser.onChunk) _parser.onChunk(off, len, header)
    }

    this.reset = function () {
      current = new HYBIMessage()
      pos = 0
      bpos = 0
      _complete = false
      _inheader = true
      _payload16 = false
      _payload64 = false
    }

    _parser.execute = function (buffer, start, end) {
      var toread, cbyte
      while (start < end) {
        if (_inheader) {
          cbyte = buffer[start++]
          switch (pos) {
            case 0:
              _payload16 = false
              _payload64 = false
              _complete = false
              current.FIN = cbyte >> 7 & 0x01
              current.RSV1 = cbyte >> 6 & 0x01
              current.RSV2 = cbyte >> 5 & 0x01
              current.RSV3 = cbyte >> 4 & 0x01
              current.OpCode = cbyte & 0x0f
              current.maskkey = [0, 0, 0, 0]
              break
            case 1:
              current.mask = cbyte >> 7 & 0x01
              current.length = cbyte & 0x7f
              if (current.length === 126) {
                _payload16 = true
              } else if (current.length === 127) {
                _payload64 = true
              } else if (!current.length) {
                onMessage()
              } else if (!current.mask) {
                _inheader = false
                bpos = 0
                onHeader()
              }
              break
            case 2:
              if (_payload16) {
                current.length = cbyte << 8
              } else if (!_payload64) {
                current.maskkey[0] = cbyte
              }
              break
            case 3:
              if (_payload16) {
                current.length += cbyte
                if (!current.mask) {
                  if (current.length) {
                    _inheader = false
                    bpos = 0
                    onHeader()
                  } else {
                    onMessage()
                  }
                }
              } else if (_payload64) {
                current.length = cbyte << 48
              } else {
                current.maskkey[1] = cbyte
              }
              break
            case 4:
              if (_payload16) {
                current.maskkey[0] = cbyte
              } else if (_payload64) {
                current.length += cbyte << 40
              } else {
                current.maskkey[2] = cbyte
              }
              break
            case 5:
              if (_payload16) {
                current.maskkey[1] = cbyte
              } else if (_payload64) {
                current.length += cbyte << 32
              } else {
                current.maskkey[3] = cbyte
                if (current.length) {
                  _inheader = false
                  bpos = 0
                  onHeader()
                } else {
                  onMessage()
                }
              }
              break
            case 6:
              if (_payload16) {
                current.maskkey[2] = cbyte
              } else if (_payload64) {
                current.length += cbyte << 24
              }
              break
            case 7:
              if (_payload16) {
                current.maskkey[3] = cbyte
                if (current.length) {
                  _inheader = false
                  bpos = 0
                  onHeader()
                } else {
                  onMessage()
                }
              } else if (_payload64) {
                current.length += cbyte << 16
              }
              break
            case 8:
              if (_payload64) {
                current.length += cbyte << 8
              }
              break
            case 9:
              if (_payload64) {
                current.length += cbyte
                if (current.mask === 0) {
                  if (current.length) {
                    _inheader = false
                    bpos = 0
                    onHeader()
                  } else {
                    onMessage()
                  }
                }
              }
              break
            case 10:
              if (_payload64) {
                current.maskkey[0] = cbyte
              }
              break
            case 11:
              if (_payload64) {
                current.maskkey[1] = cbyte
              }
              break
            case 12:
              if (_payload64) {
                current.maskkey[2] = cbyte
              }
              break
            case 13:
              if (_payload64) {
                current.maskkey[3] = cbyte
                if (current.length) {
                  _inheader = false
                  bpos = 0
                  onHeader()
                } else {
                  onMessage()
                }
              }
              break
            default:
              // error
              break
          }
          if (!_complete) {
            pos++
          } else {
            _complete = false
          }
        } else {
          toread = current.length - bpos
          if (toread === 0) {
            _inheader = true
            onMessage()
          } else if (toread <= end - start) {
            onChunk(start, toread, current)
            start += toread
            bpos += toread
            onMessage()
            _inheader = true
          } else {
            toread = end - start
            onChunk(start, toread, current)
            start += toread
            bpos += toread
          }
        }
      }
    }
  }

  function createMessage (str) {
    const OpCode = 0x81
    const dataLength = str.length
    let startOffset = 2
    let secondByte = dataLength
    let i = 0
    // todo: hmmm....
    if (dataLength > 65536) {
      startOffset = 10
      secondByte = 127
    } else if (dataLength > 125) {
      startOffset = 4
      secondByte = 126
    }
    const buf = new ArrayBuffer(startOffset + str.length)
    const bytes = new Uint8Array(buf)
    bytes[0] = OpCode
    bytes[1] = secondByte | 0
    switch (secondByte) {
      case 126:
        bytes[2] = dataLength >>> 8
        bytes[3] = dataLength % 256
        break
      case 127:
        var l = dataLength
        for (i = 1; i <= 8; ++i) {
          bytes[startOffset - i] = l & 0xff
          l >>>= 8
        }
        break
    }
    buf.writeString(str, startOffset)
    return buf
  }

  function unmask (bytes, maskkey, off, len) {
    let size = len
    let pos = off
    while (size--) {
      bytes[pos] = bytes[pos] ^ maskkey[(pos - off) % 4]
      pos++
    }
  }

  return { Parser, createMessage, unmask }
}

function createInspector (host = '127.0.0.1', port = 9222, onReady) {
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
      const str = chunks.join('')
      global.send(str)
      chunks.length = 0
      if (JSON.parse(str).method === 'Runtime.runIfWaitingForDebugger') {
        onReady()
        // this will block on the main event loop
        // todo. won't this be a problem if we are in middle of a chunk
        // of data?
      }
    }
    request.onData = (off, len) => {
      request.parser.execute(new Uint8Array(rbuf, off, len), 0, len)
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
      title: just.args[1],
      type: 'node',
      url: `file://${just.sys.cwd()}/${just.args[1]}`,
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

  global.receive = message => {
    net.send(clientfd, ws.createMessage(message))
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

  just.inspector.enable()

  const { crypto, encode, sys, net, http } = just
  const ws = wsModule()
  const websockets = {}
  const BUFSIZE = 1 * 1024 * 1024
  const { EPOLLIN, EPOLLERR, EPOLLHUP } = just.loop
  const { SOMAXCONN, O_NONBLOCK, SOCK_STREAM, AF_INET, SOCK_NONBLOCK, SOL_SOCKET, SO_REUSEADDR, SO_REUSEPORT, IPPROTO_TCP, TCP_NODELAY, SO_KEEPALIVE } = net
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
    info: () => {
      return { paused }
    }
  }
}

function main () {
  const pathMod = pathModule()
  const stringify = (o, sp = '  ') => JSON.stringify(o, (k, v) => (typeof v === 'bigint') ? v.toString() : v, sp)
  just.factory = factory
  global.loop = factory.create(1024)
  just.memoryUsage = wrapMemoryUsage(sys.memoryUsage)
  just.cpuUsage = wrapCpuUsage(sys.cpuUsage)
  just.hrtime = wrapHrtime(sys.hrtime)
  just.env = wrapEnv(sys.env)
  just.require = wrapRequire({}, pathMod).require
  just.heapUsage = wrapHeapUsage(sys.heapUsage)
  just.path = pathMod
  just.createLoop = createLoop
  let waitForInspector = false
  just.args = just.args.filter(arg => {
    const found = (arg === '--inspector')
    if (found) waitForInspector = true
    return !found
  })
  const { args } = just
  ArrayBuffer.prototype.writeString = function(str, off = 0) { // eslint-disable-line
    return sys.writeString(this, str, off)
  }
  ArrayBuffer.prototype.readString = function (len, off) { // eslint-disable-line
    return sys.readString(this, len, off)
  }
  ArrayBuffer.fromString = str => sys.calloc(1, str)
  // we are in a thread
  if (just.workerSource) {
    const source = just.workerSource
    delete just.workerSource
    // script name is passed as args[0] from the runtime when we are running
    // a thread
    vm.runScript(source, args[0])
    just.factory.run()
    return
  }
  // no args passed - run a repl
  if (args.length === 1) {
    const buf = new ArrayBuffer(4096)
    repl(global.loop, buf, expr => {
      try {
        if (expr === '.exit') {
          global.loop.remove(0)
          net.close(0)
          return
        }
        const result = vm.runScript(expr, 'repl')
        if (result) {
          net.write(1, buf, buf.writeString(`${stringify(result, 2)}\n`))
        }
        net.write(1, buf, buf.writeString('\x1B[32m>\x1B[0m '))
      } catch (err) {
        net.write(1, buf, buf.writeString(`${err.stack}\n`))
        net.write(1, buf, buf.writeString('\x1B[32m>\x1B[0m '))
      }
    })
    net.write(1, buf, buf.writeString('\x1B[32m>\x1B[0m '))
    factory.run()
    return
  }
  // evaluate script from args
  if (args[1] === '-e') {
    vm.runScript(args[2], 'eval')
    factory.run()
    return
  }
  // evaluate script piped to stdin
  if (args[1] === '--') {
    const buf = new ArrayBuffer(4096)
    const chunks = []
    let bytes = net.read(0, buf)
    while (bytes > 0) {
      chunks.push(buf.readString(bytes))
      bytes = net.read(0, buf)
    }
    vm.runScript(chunks.join(''), 'stdin')
    factory.run()
    return
  }
  if (waitForInspector) {
    just.print('waiting for inspector...')
    global.inspector = createInspector('127.0.0.1', 9222, () => {
      vm.runScript(fs.readFile(args[1]), pathMod.join(sys.cwd(), args[1]))
    })
  } else {
    vm.runScript(fs.readFile(args[1]), pathMod.join(sys.cwd(), args[1]))
  }
  factory.run()
}

main()
