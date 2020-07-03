const { createServer } = just.require('./transport.js')
const { createParser } = just.require('./protocol.js')

function onConnect (sock) {
  const parser = sock.parser = createParser(new ArrayBuffer(BUFSIZE))
  const { buf, size } = responses[200]
  parser.onRequests = count => {
    for (let i = 0; i < count; i++) {
      const url = parser.url(i)
    }
    sock.write(buf, size * count)
  }
  const { buffer } = parser
  sock.buffer = buffer
  sock.onData = bytes => parser.parse(bytes)
  sock.onClose = () => {}
}

const maxPipeline = 256
const BUFSIZE = 65536
const r200 = 'HTTP/1.1 200 OK\r\nServer: V\r\nDate: Sat, 27 Jun 2020 01:11:23 GMT\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!'
const responses = {
  200: {
    buf: ArrayBuffer.fromString(r200.repeat(maxPipeline)),
    size: r200.length
  }
}
const server = createServer()
server.onConnect = onConnect
server.listen()

const last = { user: 0, system: 0 }

just.setInterval(() => {
  const bw = 1000 * 1000 * 1000
  const { rss } = just.memoryUsage()
  const { user, system } = just.cpuUsage()
  const upc = ((user - last.user) / 1000000).toFixed(2)
  const spc = ((system - last.system) / 1000000).toFixed(2)
  const { conn, rps, wps } = server.stats
  just.print(`mem ${rss} conn ${conn} cpu ${upc} / ${spc} Gbps  r ${((rps * 8) / bw).toFixed(2)} w ${((wps * 8) / bw).toFixed(2)}`)
  last.user = user
  last.system = system
  last.user = user
  last.system = system
  server.stats.rps = server.stats.wps = 0
}, 1000)
