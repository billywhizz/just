const fd = just.fs.open('/dev/zero')
const BUFSIZE = 64 * 1024
const buf = just.sys.calloc(1, BUFSIZE)
let r = 0
let total = 0
let bytes = 0
const target = parseInt(just.args[2] || '1', 10)
const mem = new Float64Array(16)

function toMib (bytes) {
  return Math.floor(bytes / (1024 * 1024)) * 8
}

function run (target) {
  const start = Date.now()
  while (1) {
    r = just.net.read(fd, buf)
    if (r < 0) {
      just.print(just.sys.errno())
    } else {
      total++
      bytes += r
    }
    if (total === target) {
      just.print(bytes)
      const elapsed = Date.now() - start
      just.print(toMib(bytes / (elapsed / 1000)))
      break
    }
  }
  total = 0
  bytes = 0
  just.print(just.sys.memoryUsage(mem)[0])
  gc()
}

run(target)
run(target)
run(target)
run(target)
run(target)
run(target)
run(target)
run(target)
run(target)
run(target)

just.net.close(fd)

// 128 Gib/sec
