const { HTTPStream } = just.require('./http.js')

const buf = ArrayBuffer.fromString('GET / HTTP/1.1\r\nHost: foo\r\n\r\n'.repeat(3640))
const stream = new HTTPStream(buf, 256)
const err = stream.parse(buf.byteLength, count => {
  just.print(count)
  const { offsets } = stream
  for (let i = 0; i < count; i++) {
    const index = i * 2
    just.print(offsets[index + 1])
    just.print(offsets[index])
    just.print(stream.getHeaders(i))
  }
})
just.print(err)
