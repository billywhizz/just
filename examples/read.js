const { print, net, sys, fs, args } = just
const { calloc } = sys
const { read, close } = net

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

const memoryUsage = wrapMemoryUsage(sys.memoryUsage)

const fd = fs.open('/dev/zero')
const BUFSIZE = 256 * 1024
const buf = calloc(1, BUFSIZE)
let r = 0
let total = 0
let bytes = 0
const target = parseInt(args[2] || '1', 10)

function toMib (bytes) {
  return Math.floor(bytes / (1024 * 1024)) * 8
}

function run (target) {
  const start = Date.now()
  while (1) {
    r = read(fd, buf)
    if (r < 0) {
      print(just.sys.errno())
    } else {
      total++
      bytes += r
    }
    if (total === target) {
      print(toMib(bytes / ((Date.now() - start) / 1000)))
      break
    }
  }
  total = 0
  bytes = 0
  print(JSON.stringify(memoryUsage(), null, '  '))
}

run(target)

close(fd)

// 147 Gib/sec with 256k chunks
