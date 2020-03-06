const { zlib, net, sys } = just

const BUFSIZE = 16

const buf = new ArrayBuffer(BUFSIZE)
const inflate = zlib.createInflate(buf, BUFSIZE, 31)
let len = net.read(0, buf)
let written = 0
while (len > 0) {
  written = zlib.writeInflate(inflate, len, zlib.Z_FULL_FLUSH)
  just.error(`written: ${written}`)
  if (written === -5) {
    just.print('foo')
  }
  if (written < 0) throw new Error(`writeInflate: ${written}`)
  if (written > 0) {
    written = net.write(1, inflate, written)
    if (written < 0) throw new Error(`net.write: ${written} (${sys.errno()}) ${sys.strerror(sys.errno())}`)
  }
  len = net.read(0, buf)
}
just.error(`read: ${len}`)
written = zlib.writeInflate(inflate, 0, zlib.Z_FINISH)
if (written === -5) {
  just.error('foo')
}
if (written < 0) throw new Error(`writeInflate: ${written}`)
if (written > 0) {
  written = net.write(1, inflate, written)
  if (written < 0) throw new Error(`net.write: ${written} (${sys.errno()}) ${sys.strerror(sys.errno())}`)
}
zlib.endInflate(inflate, true)

just.error(just.memoryUsage().rss)
