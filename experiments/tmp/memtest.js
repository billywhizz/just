
const { sys } = just

const EOH = 168626701 // CrLfCrLf as a 32 bit unsigned integer

function findChar1 (source, dest, off) {
  return sys.findChar(source, dest, off)
}

function findChar2 (source, dest, off) {
  if (off === len) {
    return -1
  }
  for (let i = off; i < (len - off) - 3; i++) {
    const next4 = dv.getUint32(i, true)
    if (next4 === dest) {
      return i - off
    }
  }
}

const source = ArrayBuffer.fromString('GET / HTTP/1.1\r\nHost: foo\r\n\r\n'.repeat(64))
const len = source.byteLength
const dv = new DataView(source)
const dest = ArrayBuffer.fromString('\r\n\r\n')
just.print(source.byteLength)

just.setInterval(() => {
  just.print(found)
  found = 0
}, 1000)

let found = 0
while (1) {
  for (let i = 0; i < 1000; i++) {
    //let off = findChar1(source, dest, 0)
    let off = findChar2(source, EOH, 0)
    while (off >= 0) {
      just.print(off)
      found++
      off += 4
      //const next = findChar1(source, dest, off)
      const next = findChar2(source, EOH, off)
      if (next === -1) break
      off += next
      just.sys.usleep(100000)
    }
  }
  just.factory.loop.poll(0)
  just.sys.runMicroTasks()
}
