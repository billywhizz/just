const { HTTPStream } = just.require('./http.js')
const { createServer } = just.require('./net.js')

const maxPipeline = 512
let rps = 0

function onConnect (sock) {
  const stream = new HTTPStream(sock.buf, maxPipeline)
  const { buf, size } = responses[200]
  sock.onReadable = () => stream.parse(sock.pull(stream.offset), count => {
    rps += count
    sock.write(buf, count * size)
  })
  sock.onEnd = () => {}
}

const r200 = 'HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n'
const r404 = 'HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n'

const responses = {
  200: { buf: ArrayBuffer.fromString(r200.repeat(maxPipeline)), size: r200.length },
  404: { buf: ArrayBuffer.fromString(r404.repeat(maxPipeline)), size: r404.length }
}

just.setInterval(() => {
  const { rss } = just.memoryUsage()
  just.print(`mem ${rss} rps ${rps}`)
  rps = 0
}, 1000)

createServer(onConnect).listen()
