
const { createParser } = just.require('./parser.js')

class HTTPStream {
  constructor (buf = new ArrayBuffer(4096), maxPipeline = 256, parser = createParser(buf, maxPipeline), off = 0) {
    this.buf = buf
    this.parser = parser
    this.offset = off
    this.inHeader = false
    this.bodySize = 0
    this.bodyBytes = 0
  }

  parse (bytes, onRequests) {
    if (bytes === 0) return
    const { buf, parser } = this
    let { offset } = this
    const size = offset + bytes
    const { count, off } = parser.parse(size)
    if (count > 0) {
      onRequests(count)
      if (off < (size)) {
        buf.copyFrom(buf, 0, size - off, off)
        offset = size - off
      } else {
        offset = 0
      }
    } else {
      if (size === buf.byteLength) {
        return -3
      }
      offset = size
    }
    this.offset = offset
    return offset
  }

  getHeaders (index) {
    const { buf, parser } = this
    const { offsets } = parser
    index *= 2
    return buf.readString(offsets[index + 1], offsets[index])
  }
}

module.exports = { HTTPStream }
