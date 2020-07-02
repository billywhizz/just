const { createClient } = just.require('./lib/net.js')
const { loop } = just.factory
const { sys, setTimeout, setInterval, clearTimeout, http } = just
const { runMicroTasks, nextTick } = sys

function onComplete () {
  just.print('done')
  running = false
  clearTimeout(timer)
}

function onStats () {
  const { rss } = just.memoryUsage()
  const { user, system } = just.cpuUsage()
  const upc = (user - last.user) / 1000000
  const spc = (system - last.system) / 1000000
  just.print(`mem ${rss} cpu ${upc.toFixed(2)} / ${spc.toFixed(2)} rps ${rps}`)
  last.user = user
  last.system = system
  rps = 0
}

function onConnect (sock) {
  sock.onReadable = () => {
    let bytes = sock.pull(0)
    let off = 0
    let nread = http.parseResponse(sock.buf, bytes, off)
    while (nread > 0) {
      rps++
      bytes -= nread
      off += nread
      nread = http.parseResponse(sock.buf, bytes, off)
    }
    //http.parseResponse(sock.buf, sock.pull(0), 0)
    //const response = http.getResponse()
    //just.print(JSON.stringify(response))
    //just.print(size)
  }
  sock.onWritable = () => {
    sockets[sock.fd] = sock
  }
  sock.onEnd = () => {
    delete sockets[sock.fd]
  }
}

function shutdown () {
  for (const fd of Object.keys(sockets)) {
    sockets[fd].close()
  }
}

function run () {
  for (const fd of Object.keys(sockets)) {
    const socket = sockets[fd]
    const written = socket.write(buf, buf.byteLength)
    if (written < buf.byteLength) {
      sockets[fd].close()
      delete sockets[fd]
    }
  }
  loop.poll(0)
  runMicroTasks()
  if (running) return nextTick(run)
  shutdown()
}

let rps = 0
let numclients = parseInt(just.args[2] || '1', 10)
const maxPipeline = parseInt(just.args[3] || '1', 10)
const duration = parseInt(just.args[4] || '10', 10)
const sockets = {}
const buf = ArrayBuffer.fromString('GET / HTTP/1.1\r\n\r\n'.repeat(maxPipeline))
while (numclients--) createClient(onConnect).connect()
let running = true
setTimeout(onComplete, duration * 1000)
const timer = setInterval(onStats, 1000)
const last = { user: 0, system: 0 }
run()
