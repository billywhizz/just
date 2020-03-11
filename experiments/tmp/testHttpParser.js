const { HTTPStream } = just.require('./http.js')

const buf = new ArrayBuffer(16384)
const stream = new HTTPStream(buf)

let len = buf.writeString('GET / HTTP/1.1\r\n\r\n')
let offset = stream.parse(len, count => {
  just.print(`${count} requests`)
})
just.print(offset)

len = buf.writeString('GET / HTTP/1.1', offset)
offset = stream.parse(len, count => {
  just.print(`${count} requests`)
})
just.print(offset)

len = buf.writeString('\r\nHost: foo\r\n\r\n', offset)
offset = stream.parse(len, count => {
  just.print(`${count} requests`)
})
just.print(offset)

len = buf.writeString('GET / HTTP/1.1', offset)
offset = stream.parse(len, count => {
  just.print(`${count} requests`)
})
just.print(offset)

len = buf.writeString('\r\n\r\n', offset)
offset = stream.parse(len, count => {
  just.print(`${count} requests`)
})
just.print(offset)
