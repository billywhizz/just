const { createHTTPStream, HTTPStream } = just.require('./http.js')
const { loop } = just.factory
const buf = ArrayBuffer.fromString('GET / HTTP/1.1\r\nHost: foo\r\n\r\n'.repeat(256))
const stream = new HTTPStream(buf)
//const stream = createHTTPStream(buf)
const len = buf.byteLength
just.setInterval(() => {
  just.print(total)
  total = 0
}, 1000)
let total = 0
while (1) {
  for (let i = 0; i < 10000; i++) {
    stream.parse(len, count => {
      total += count
    })
  }
  loop.poll(0)
  just.sys.runMicroTasks()
}
