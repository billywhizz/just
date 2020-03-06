const { zlib, net, sys } = just

const BUFSIZE = 64 * 1024

const buf = new ArrayBuffer(BUFSIZE)
const deflate = zlib.createDeflate(buf, BUFSIZE, zlib.Z_DEFAULT_COMPRESSION, 31)
let len = net.read(0, buf)
let written = 0
while (len > 0) {
  written = zlib.writeDeflate(deflate, len, zlib.Z_NO_FLUSH)
  if (written < 0) throw new Error(`writeDeflate: ${written}`)
  if (written > 0) {
    written = net.write(1, deflate, written)
    if (written < 0) throw new Error(`net.write: ${written} (${sys.errno()}) ${sys.strerror(sys.errno())}`)
  }
  len = net.read(0, buf)
}
written = zlib.writeDeflate(deflate, 0, zlib.Z_FINISH)
if (written < 0) throw new Error(`writeDeflate: ${written}`)
if (written > 0) {
  written = net.write(1, deflate, written)
  if (written < 0) throw new Error(`net.write: ${written} (${sys.errno()}) ${sys.strerror(sys.errno())}`)
}
zlib.endDeflate(deflate, true)

just.error(`gzip rss: ${just.memoryUsage().rss}`)
