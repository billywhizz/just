const { args, vm, fs, sys, net } = just

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
    EPOLL_CTL_DEL, EPOLL_CTL_MOD, EPOLLIN, EPOLLOUT, EPOLLET 
  } = just.loop
  const evbuf = just.sys.calloc(nevents, 12)
  const events = new Uint32Array(evbuf)
  const loopfd = create(EPOLL_CLOEXEC)
  const handles = {}
  function poll (timeout = -1) {
    const r = wait(loopfd, evbuf, timeout)
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

function setFlag (fd, flag) {
  let flags = sys.fcntl(fd, sys.F_GETFL, 0)
  flags |= flag
  return sys.fcntl(fd, sys.F_SETFL, flags)
}

function repl (loop, buf, onExpression) {
  const { EPOLLIN } = just.loop
  const { O_NONBLOCK, EAGAIN } = just.net
  const stdin = 0
  setFlag(stdin, O_NONBLOCK)
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
      onExpression(sys.readString(buf, bytes).trim())
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
    dirName = baseName(fileName)
    const params = ['exports', 'require', 'module']
    const exports = {}
    const module = { exports, dirName, fileName }
    module.text = just.fs.readFile(fileName)
    const fun = just.vm.compile(module.text, fileName, params, [])
    module.function = fun
    fun.call(exports, exports, p => require(p, module), module)
    return module.exports
  }
  return { cache, require }
}

function main () {
  const pathMod = pathModule()
  const stringify = (o, sp = '  ') => JSON.stringify(o, (k, v) => (typeof v === 'bigint') ? v.toString() : v, sp)
  just.memoryUsage = wrapMemoryUsage(sys.memoryUsage)
  just.cpuUsage = wrapCpuUsage(sys.cpuUsage)
  just.hrtime = wrapHrtime(sys.hrtime)
  just.env = wrapEnv(sys.env)
  just.require = wrapRequire({}, pathMod).require
  just.heapUsage = wrapHeapUsage(sys.heapUsage)
  just.path = pathMod
  just.createLoop = createLoop
  if (args.length === 1) {
    const buf = sys.calloc(1, 4096)
    const loop = createLoop()
    repl(loop, buf, expr => {
      try {
        if (expr === '.exit') {
          loop.remove(0)
          net.close(0)
          return
        }
        const result = vm.runScript(expr, 'repl')
        if (result) {
          net.write(1, buf, sys.writeString(buf, `${stringify(result, 2)}\n`))
        }
        net.write(1, buf, sys.writeString(buf, '> '))
      } catch (err) {
        net.write(1, buf, sys.writeString(buf, `${err.stack}\n`))
      }
    })
    net.write(1, buf, sys.writeString(buf, '> '))
    while (loop.count > 0) {
      loop.poll(10)
      sys.runMicroTasks()
    }
    return
  } else if (args[1] === '-e') {
    vm.runScript(args[2], 'eval')
    return
  }
  vm.runScript(fs.readFile(args[1]), args[1])
}

main()
