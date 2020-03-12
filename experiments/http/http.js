
const EOH = 168626701 // CrLfCrLf as a 32 bit unsigned integer
const MIN_REQUEST_SIZE = 18

class HTTPStream {
  constructor (buf = new ArrayBuffer(4096), maxPipeline = 256, off = 0) {
    this.buf = buf
    this.offset = off
    this.inHeader = false
    this.bodySize = 0
    this.bodyBytes = 0
    this.dv = new DataView(buf)
    this.bufLen = buf.byteLength
    const maxReq = Math.floor(buf.byteLength / MIN_REQUEST_SIZE)
    this.offsets = new Uint16Array(maxReq * 2)
  }

  parse (bytes, onRequests) {
    if (bytes === 0) return
    const { buf, bufLen, dv, offsets } = this
    let { offset } = this
    const size = offset + bytes
    const len = Math.min(size, bufLen)
    let off = 0
    let count = 0
    for (let i = 0; i < len - 3; i++) {
      const next4 = dv.getUint32(i, true)
      if (next4 === EOH) {
        const index = count * 2
        offsets[index] = off
        offsets[index + 1] = i + 4 - off
        off = i + 4
        count++
        // todo: check for exceeding maxPipeline
      }
    }
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
    const { buf, offsets } = this
    index *= 2
    return buf.readString(offsets[index + 1], offsets[index])
  }
}

module.exports = { HTTPStream }
