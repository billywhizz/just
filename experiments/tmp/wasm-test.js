const { compile, evaluate, save, createMemory } = just.require('wasm')

async function main () {
  const fileName = just.path.join(just.path.baseName(just.path.scriptName), './test.wat')
  just.print(fileName)
  const { wasm } = await compile(fileName)
  save('./test.wasm', wasm)
  let callbacks = 0
  const context = {
    callback: (off, len) => {
      callbacks++
    }
  }
  const memory = createMemory({ initial: 4 })
  const { buffer } = memory
  const { parse } = evaluate(wasm, context, memory)
  const str = 'GET / HTTP/1.1\r\nHost: foo\r\n\r\n'.repeat(100 * 64)
  const len = buffer.writeString(str)
  just.print(len)
  function test () {
    callbacks = 0
    let runs = 1000
    let total = 0
    const start = Date.now()
    while (runs--) {
      total += parse(0, len, len)
    }
    const elapsed = Date.now() - start
    const rate = (total / (elapsed / 1000))
    just.print(`done ${total} time ${elapsed} rate ${rate} callbacks ${callbacks}`)
    just.print(just.memoryUsage().rss)
  }
  while (1) {
    test()
    just.sys.runMicroTasks()
  }
}

main().catch(err => just.error(err.stack))
