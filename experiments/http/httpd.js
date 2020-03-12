const { HTTPStream } = just.require('./http.js')
const { createServer } = just.require('./net.js')
const { sys, net } = just

const maxPipeline = 256
let rps = 0
let tbytes = 0
let conn = 0

function onConnect (sock) {
  conn++
  const stream = new HTTPStream(sock.buf, maxPipeline)
  const { buf, size } = responses[200]
  sock.onReadable = () => {
    const bytes = sock.pull(stream.offset)
    if (bytes <= 0) return
    const err = stream.parse(bytes, count => {
      rps += count
      const r = sock.write(buf, count * size)
      if (r < 0) {
        const errno = sys.errno()
        if (errno === net.EAGAIN) return sock.pause()
        just.print(`write: (${errno}) ${sys.strerror(errno)}`)
        return sock.close()
      }
      if (r === 0) {
        just.print('zero bytes')
        return sock.close()
      }
      tbytes += bytes
    })
    if (err < 0) just.print(`error: ${err}`)
  }
  sock.onWritable = () => {
    just.print('writable')
  }
  sock.onEnd = () => conn--
}

const r200 = 'HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n'
const r404 = 'HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n'

const responses = {
  200: { buf: ArrayBuffer.fromString(r200.repeat(maxPipeline)), size: r200.length },
  404: { buf: ArrayBuffer.fromString(r404.repeat(maxPipeline)), size: r404.length }
}

const last = { user: 0, system: 0 }

just.setInterval(() => {
  const { rss } = just.memoryUsage()
  const { user, system } = just.cpuUsage()
  const upc = (user - last.user) / 1000000
  const spc = (system - last.system) / 1000000
  just.print(`mem ${rss} conn ${conn} rps ${rps} cpu ${upc.toFixed(2)} / ${spc.toFixed(2)} bytes ${tbytes}`)
  last.user = user
  last.system = system
  rps = tbytes = 0
}, 1000)

createServer(onConnect).listen()
