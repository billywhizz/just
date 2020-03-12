const { compile, evaluate, save, createMemory } = just.require('wasm')

async function main () {
  const fileName = just.path.join(just.path.baseName(just.path.scriptName), './parse.wat')
  const { wasm } = await compile(fileName)
  const memory = createMemory({ initial: 16 })
  const { buffer } = memory
  let requests = 0
  const CR = 13
  const EOH = ArrayBuffer.fromString('\r\n\r\n')
  const view = new DataView(buffer)
  const u8 = new Uint8Array(buffer)
  function callback (off) {
    requests++
  }
  function parse3 (start, end) {
    let off = just.sys.findChar(buffer, EOH, start)
    while (off >= 0) {
      start += off
      callback(start)
      start++
      off = just.sys.findChar(buffer, EOH, start)
    }
    return 0
  }

  function parse2 (start, end) {
    end -= 3
    while (start < end) {
      if (u8[start] === CR) {
        if (view.getUint32(start, true) === 168626701) {
          callback(start)
          start += 3
        }
      }
      start++
    }
  }
  const context = { callback }
  const { parse } = evaluate(wasm, context, memory)
  const str = 'GET / HTTP/1.1\r\nHost: foo\r\n\r\n'.repeat(1000)
  const len = buffer.writeString(str, 100)
  just.print(len)
  function test () {
    const then = Date.now()
    for (let i = 0; i < 10000; i++) {
      let off = 100
      while (off > 0) {
        off = parse3(off, len + off)
      }
    }
    const elapsed = Date.now() - then
    just.print(requests / (elapsed / 1000))
  }
  while (1) {
    test()
    requests = 0
    just.sys.runMicroTasks()
  }
}

main().catch(err => just.error(err.stack))
