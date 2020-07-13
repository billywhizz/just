const { zlib } = just

const str = 'hellohellohellohello'
const buf = ArrayBuffer.fromString(str)
const crc32 = zlib.crc32(buf, buf.byteLength, 0)
just.print(crc32)

const str2 = 'hellohello'
const buf2 = ArrayBuffer.fromString(str2)
let crc32a = zlib.crc32(buf2, buf2.byteLength, 0)
just.print(crc32a)
crc32a = zlib.crc32(buf2, buf2.byteLength, crc32a)
just.print(crc32a.toString(16))
