const { http } = just

function createParser (buf) {
  const bufLen = buf.byteLength
  const state = { count: 0, off: 0 }
  function parse (len = bufLen) {
    len = Math.min(len, bufLen)
    let off = 0
    let count = 0
    let nread = http.parseRequest(buf, len, off)
    while (nread > 0) {
      off += nread
      len -= nread
      count++
      nread = http.parseRequest(buf, len, off)
    }
    state.count = count
    state.off = off
    return state
  }
  return { parse }
}

module.exports = { createParser }
