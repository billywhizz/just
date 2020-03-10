const { http } = just
const MAX_PIPELINE = 512

function createParser (buf, maxPipeline = MAX_PIPELINE) {
  const bufLen = buf.byteLength
  const state = { count: 0, off: 0 }
  const offsets = new Uint16Array(maxPipeline * 2)
  function parse (len = bufLen) {
    len = Math.min(len, bufLen)
    let off = 0
    let count = 0
    let nread = http.parseRequest(buf, len, off)
    while (nread > 0) {
      const index = count * 2
      offsets[index] = off
      offsets[index + 1] = nread
      off += nread
      len -= nread
      count++
      if (count === maxPipeline) {
        state.error = -1
        return state
      }
      nread = http.parseRequest(buf, len, off)
    }
    state.count = count
    state.off = off
    return state
  }
  return { offsets, state, parse }
}

module.exports = { createParser }
