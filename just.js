function wrapHrtime (hrtime) {
  const time = new BigUint64Array(1)
  return () => {
    hrtime(time)
    return time[0]
  }
}

function wrapCpuUsage (cpuUsage) {
  const cpu = new Float64Array(16)
  return () => {
    cpuUsage(cpu)
    return {
      user: cpu[0],
      system: cpu[1]
    }
  }
}

function wrapMemoryUsage (memoryUsage) {
  const mem = new Float64Array(16)
  return () => {
    memoryUsage(mem)
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

function wrapRequireNative (cache = {}) {
  function require (path) {
    if (cache[path]) return cache[path].exports
    const params = ['exports', 'require', 'module']
    const exports = {}
    const module = { exports, type: 'native' }
    if (path === 'inspector') {
      module.text = just.vm.builtin(just.vm.INSPECTOR).slice(0)
    } else if (path === 'websocket') {
      module.text = just.vm.builtin(just.vm.WEBSOCKET).slice(0)
    } else if (path === 'require') {
      module.text = just.vm.builtin(just.vm.REQUIRE).slice(0)
    } else if (path === 'path') {
      module.text = just.vm.builtin(just.vm.PATH).slice(0)
    } else if (path === 'loop') {
      module.text = just.vm.builtin(just.vm.LOOP).slice(0)
    } else if (path === 'repl') {
      module.text = just.vm.builtin(just.vm.REPL).slice(0)
    }
    if (!module.text) return
    const fun = just.vm.compile(module.text, path, params, [])
    module.function = fun
    cache[path] = module
    fun.call(exports, exports, p => require(p, module), module)
    return module.exports
  }
  return { cache, require }
}

function main () {
  const { vm, fs, sys, net } = just
  // add helpers to ArrayBuffer class. TODO: this is probably a bad thing
  ArrayBuffer.prototype.writeString = function(str, off = 0) { // eslint-disable-line
    return sys.writeString(this, str, off)
  }
  ArrayBuffer.prototype.readString = function (len, off) { // eslint-disable-line
    return sys.readString(this, len, off)
  }
  ArrayBuffer.prototype.copyFrom = function (ab, off, len, off2 = 0) { // eslint-disable-line
    return sys.memcpy(this, ab, off, len, off2)
  }
  const calloc = sys.calloc
  ArrayBuffer.fromString = str => calloc(1, str)
  delete sys.calloc // remove this as we don't want it in userland
  just.memoryUsage = wrapMemoryUsage(sys.memoryUsage)
  just.cpuUsage = wrapCpuUsage(sys.cpuUsage)
  just.hrtime = wrapHrtime(sys.hrtime)
  just.env = wrapEnv(sys.env)
  just.requireCache = {}
  just.require = just.requireNative = wrapRequireNative(just.requireCache).require
  just.require = just.require('require').wrap(just.requireCache).require
  just.heapUsage = wrapHeapUsage(sys.heapUsage)
  just.path = just.require('path')
  // todo: factory is a bad name
  const { factory, createLoop } = just.require('loop')
  just.factory = factory
  global.loop = factory.create(1024)
  just.createLoop = createLoop
  let waitForInspector = false
  just.args = just.args.filter(arg => {
    const found = (arg === '--inspector')
    if (found) waitForInspector = true
    return !found
  })
  const { args } = just
  // we are in a thread
  if (just.workerSource) {
    const source = just.workerSource
    delete just.workerSource
    // script name is passed as args[0] from the runtime when we are running
    // a thread
    vm.runScript(source, args[0])
    factory.run()
    return
  }
  // no args passed - run a repl
  if (args.length === 1) {
    just.require('repl').repl(global.loop, new ArrayBuffer(4096))
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
    let bytes = net.read(sys.STDIN_FILENO, buf)
    while (bytes > 0) {
      chunks.push(buf.readString(bytes))
      bytes = net.read(sys.STDIN_FILENO, buf)
    }
    vm.runScript(chunks.join(''), 'stdin')
    factory.run()
    return
  }
  if (waitForInspector) {
    just.print('waiting for inspector...')
    global.inspector = just.require('inspector').createInspector('127.0.0.1', 9222, () => {
      vm.runScript(fs.readFile(args[1]), just.path.join(sys.cwd(), args[1]))
    })
  } else {
    vm.runScript(fs.readFile(args[1]), just.path.join(sys.cwd(), args[1]))
  }
  factory.run()
}

main()
