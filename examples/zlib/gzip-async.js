const { zlib, net } = just
const { Z_FINISH, Z_NO_FLUSH, Z_FULL_FLUSH, Z_BEST_COMPRESSION, Z_NO_COMPRESSION, Z_DEFAULT_COMPRESSION, Z_DEFAULT_WINDOW_BITS } = zlib
const output = new ArrayBuffer(4096)
let off = 10
const header = new Uint8Array(output, 0, 10)
header[0] = 0x1f
header[1] = 0x8b
header[2] = 8
header[9] = 0xff
const buf = new ArrayBuffer(4096)
let sourceLen = 0
const deflate = zlib.createDeflate(buf, 4096, Z_BEST_COMPRESSION, -Z_DEFAULT_WINDOW_BITS)
let crc32 = 0
let len = net.read(0, buf)
while (len > 0) {
  sourceLen += len
  crc32 = zlib.crc32(buf, len, crc32)
  const written = zlib.writeDeflate(deflate, len, Z_NO_FLUSH)
  if (written < 0) throw new Error(`writeDeflate: ${written}`)
  if (written > 0) off += output.copyFrom(deflate, off, written)
  len = net.read(0, buf)
}
const written = zlib.writeDeflate(deflate, 0, Z_FINISH)
if (written < 0) throw new Error(`writeDeflate: ${written}`)
if (written > 0) off += output.copyFrom(deflate, off, written)
zlib.endDeflate(deflate, true)
const footer = new DataView(output, off, 8)
footer.setUint32(0, crc32, true)
footer.setUint32(4, sourceLen, true)
off += 8
just.net.write(1, output, off)
just.error(just.memoryUsage().rss)
