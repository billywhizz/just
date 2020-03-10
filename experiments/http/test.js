const { createParser } = just.require('./parser.js')

const chunks = [
  'GET / HTTP/1.1\r\n\r\nGET / HTTP/1.1\r\n\r\nGET /',
  ' HTTP/1.1\r\n\r\n',
  'GET / HTTP/1.1\r\n\r\nGET / HTTP/1.1\r\n\r\nGET /',
  ' HTTP/1.1\r\n\r\n',
  `GET / HTTP/1.1\r\nHost: ${'0'.repeat(230)}\r\n\r\n`,
  'GET / HTTP/1.1\r\n\r\nGET / HTTP/1.1\r\n\r\nGET /',
  ' HTTP/1.1\r\n\r\n',
  'GET / HTTP/1.1\r\n\r\nGET / HTTP/1.1\r\n\r\nGET /',
  ' HTTP/1.1\r\n\r\n',
  `GET / HTTP/1.1\r\nHost: ${'0'.repeat(230)}\r\n\r\n`,
  'G',
  'E',
  'T',
  ' ',
  '/',
  ' ',
  'H',
  'T',
  'T',
  'P',
  '/',
  '1',
  '.',
  '1',
  '\r',
  '\n',
  '\r',
  '\n',
  'GET / HTTP/1.1\r\nHost: foo\r\n\r\nGET / HTTP/1.',
  '1\r\n\r\n',
  'GET / HTTP/1.1\r\n\r\nGET / HTTP/1.1\r\n\r\nGET /',
  ' HTTP/1.1\r\n\r\n',
  'GET / HTTP/1.1\r\n\r\nGET / HTTP/1.1\r\n\r\nGET /',
  ' HTTP/1.1\r\n\r\n',
  'GET / HTTP/1.1\r\n\r\nGET / HTTP/1.1\r\n\r\nGET /',
  ' HTTP/1.1\r\n\r\n',
  'GET / HTTP/1.1\r\nHost: foo\r\n\r\nGET / HTTP/1.',
  '1\r\n\r\n',
  'GET / HTTP/1.1\r\nHost: foo\r\n\r\nGET / HTTP/1.',
  '1\r\n\r\n',
  `GET / HTTP/1.1\r\nHost: ${'0'.repeat(230)}\r\n\r\n`,
  'GET / HTTP/1.1\r\nHost: foo\r\n\r\nGET / HTTP/1.',
  '1\r\n\r\n',
  'GET / HTTP/1.1\r\nHost: foo\r\n\r\nGET / HTTP/1.',
  '1\r\n',
  '\r\n',
  `GET / HTTP/1.1\r\nHost: ${'0'.repeat(230)}\r\n\r\n`,
  'GET / HTTP/1.1\r\nHost: foo\r\n\r\n',
  'G',
  'ET /',
  ' HTTP/1.1\r\nHost: foo\r\n',
  '\r\n'
]
function onTimerEvent (fd, event) {
  const rss = just.memoryUsage().rss
  just.print(`rps ${total} mem ${rss}`)
  total = 0
  just.net.read(fd, tbuf)
}

function run () {
  while (1) {
    for (let i = 0; i < 10000; i++) {
      const chunk = chunks[chunkCounter++]
      if (chunkCounter === chunks.length) chunkCounter = 0
      //just.print(`offset: ${offset} len: ${chunk.length}`)
      if (offset + chunk.length > buf.byteLength) {
        throw new Error('Request Too Big')
      }
      const len = buf.writeString(chunk, offset)
      const { count, off } = parser.parse(offset + len)
      if (count > 0) {
        total += count
        if (off < (offset + len)) {
          buf.copyFrom(buf, 0, len - off, off)
          offset = len - off
        } else {
          offset = 0
        }
      } else {
        if (offset + len === buf.byteLength) {
          throw new Error('Request Too Big')
        }
        offset = offset + len
      }
    }
    loop.poll(0)
    just.sys.runMicroTasks()
  }
}

let offset = 0
let chunkCounter = 0
let total = 0
const tbuf = new ArrayBuffer(8)
const buf = new ArrayBuffer(16384)
const parser = createParser(buf)
const timerfd = just.sys.timer(1000, 1000)
loop.add(timerfd, onTimerEvent)

run()
