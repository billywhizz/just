const { parseRequests, getRequests, getUrl } = just.http

function createParser (buffer) {
  const answer = [0]
  const parser = { buffer }
  function parse (bytes, off = 0) {
    const count = parseRequests(buffer, buffer.offset + bytes, off, answer)
    if (count > 0) {
      parser.onRequests(count)
    }
    if (answer[0] > 0) {
      const start = buffer.offset + bytes - answer[0]
      const len = answer[0]
      if (start > buffer.offset) {
        buffer.copyFrom(buffer, 0, len, start)
      }
      buffer.offset = len
      return
    }
    buffer.offset = 0
  }
  buffer.offset = 0
  parser.parse = parse
  parser.get = count => getRequests(count)
  parser.url = index => getUrl(index)
  return parser
}

module.exports = { createParser }
