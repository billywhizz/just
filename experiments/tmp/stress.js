const { createParser } = just.require('../http/parser.js')
const { loop } = just.factory

const buf = ArrayBuffer.fromString('GET / HTTP/1.1\r\nHost: foo\r\n\r\n'.repeat(64))

const parser = createParser(buf)

const r = parser.parse()
just.print(r.count)
just.print(JSON.stringify(parser.offsets.slice(0, r.count * 2)))

const tbuf = new ArrayBuffer(8)
let count = 0

function onTimerEvent (fd, event) {
  const rss = just.memoryUsage().rss
  just.print(`rps ${count} mem ${rss}`)
  count = 0
  just.net.read(fd, tbuf)
}

const timerfd = just.sys.timer(1000, 1000)
loop.add(timerfd, onTimerEvent)

function test () {
  for (let i = 0; i < 1000; i++) {
    for (let i = 0; i < 10000; i++) {
      count += parser.parse().count
    }
    loop.poll(0)
    just.sys.runMicroTasks()
  }
}

while (1) {
  test()
}
