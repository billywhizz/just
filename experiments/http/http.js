
const { createParser } = just.require('./parser.js')

class HTTPStream {
  constructor (buf = new ArrayBuffer(4096), maxPipeline = 256, parser = createParser(buf, maxPipeline), off = 0) {
    this.buf = buf
    this.parser = parser
    this.offset = off
  }

  parse (bytes, onRequests) {
    const { buf, parser } = this
    let { offset } = this
    const size = offset + bytes
    const { count, off } = parser.parse(size)
    if (count > 0) {
      onRequests(count)
      if (off < (size)) {
        buf.copyFrom(buf, 0, bytes - off, off)
        offset = bytes - off
      } else {
        offset = 0
      }
    } else {
      if (size === buf.byteLength) {
        throw new Error('Request Too Big')
      }
      offset = size
    }
    this.offset = offset
    return offset
  }
}

function createHTTPStream (buf = new ArrayBuffer(4096), maxPipeline = 256, parser = createParser(buf, maxPipeline), offset = 0) {
  function parse (bytes, onRequests) {
    const size = offset + bytes
    const { count, off } = parser.parse(size)
    if (count > 0) {
      onRequests(count)
      if (off < (size)) {
        buf.copyFrom(buf, 0, bytes - off, off)
        offset = bytes - off
      } else {
        offset = 0
      }
    } else {
      if (size === buf.byteLength) {
        throw new Error('Request Too Big')
      }
      offset = size
    }
    stream.offset = offset
    return offset
  }
  const stream = { buf, parser, offset, parse }
  return stream
}

module.exports = { HTTPStream, createHTTPStream }
